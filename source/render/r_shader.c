//=======================================================================
//			Copyright XashXT Group 2007 ©
//		 r_shader.c - shader script parsing and loading
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "const.h"

// FIXME: remove it
const char *r_skyBoxSuffix[6] = { "rt", "lf", "bk", "ft", "up", "dn" };
static texture_t		*r_internalMiptex;


typedef struct
{
	const char	*name;
	int		surfaceFlags;
	int		contents;
	bool		clearSolid;
} shaderParm_t;

typedef struct ref_script_s
{
	string		name;
	int		type;
	uint		surfaceParm;
	string		source;
	int		line;
	char		*buffer;
	size_t		size;
	struct ref_script_s	*nextHash;
} ref_script_t;

static byte		*r_shaderpool;
static ref_shader_t		r_parseShader;
static shaderStage_t	r_parseShaderStages[SHADER_MAX_STAGES];
static statement_t		r_parseShaderOps[MAX_EXPRESSION_OPS];
static float		r_parseShaderExpressionRegisters[MAX_EXPRESSION_REGISTERS];
static stageBundle_t	r_parseStageTMU[SHADER_MAX_STAGES][MAX_TEXTURE_UNITS];

static table_t		*r_tablesHashTable[TABLES_HASH_SIZE];
static ref_script_t		*r_shaderScriptsHash[SHADERS_HASH_SIZE];
static ref_shader_t		*r_shadersHash[SHADERS_HASH_SIZE];
static table_t		*r_tables[MAX_TABLES];
static int		r_numTables;
ref_shader_t		r_shaders[MAX_SHADERS];
int			r_numShaders = 0;

// NOTE: this table must match with same table in common\bsplib\shaders.c
shaderParm_t infoParms[] =
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
	{"origin",	SURF_NONE,	CONTENTS_ORIGIN,		1}, // center of rotating brushes
	{"nolightmap",	SURF_NOLIGHTMAP,	CONTENTS_NONE,		0}, // don't generate a lightmap
	{"translucent",	SURF_NONE,	CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"detail",	SURF_NONE,	CONTENTS_DETAIL,		0}, // don't include in structural bsp
	{"fog",		SURF_NOLIGHTMAP,	CONTENTS_FOG,		0}, // carves surfaces entering
	{"sky",		SURF_SKY,		CONTENTS_NONE,		0}, // emit light from environment map
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
=======================================================================

 TABLE PARSING

=======================================================================
*/
/*
=================
R_LoadTable
=================
*/
static void R_LoadTable( const char *name, bool clamp, bool snap, size_t size, float *values )
{
	table_t	*table;
	uint	hash;

	if( r_numTables == MAX_TABLES )
		Host_Error( "R_LoadTable: MAX_TABLES limit exceeds\n" );

	// fill it in
	r_tables[r_numTables++] = table = Mem_Alloc( r_shaderpool, sizeof( table_t ));
	com.strncpy( table->name, name, sizeof( table->name ));
	table->index = r_numTables - 1;
	table->clamp = clamp;
	table->snap = snap;
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
static bool R_ParseTable( script_t *script, bool clamp, bool snap )
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
	R_LoadTable( name, clamp, snap, size, values );
	return true;
}

/*
=================
R_LookupTable
=================
*/
static float R_LookupTable( int tableIndex, float index )
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

	if( table->clamp )
	{
		curIndex = bound( 0, curIndex, table->size - 1 );
		oldIndex = bound( 0, oldIndex, table->size - 1 );
	}
	else
	{
		curIndex %= table->size;
		oldIndex %= table->size;
	}

	if( table->snap ) value = table->values[oldIndex];
	else value = table->values[oldIndex] + (table->values[curIndex] - table->values[oldIndex]) * frac;

	return value;
}

/*
=======================================================================

SHADER EXPRESSION PARSING

=======================================================================
*/
#define MAX_EXPRESSION_VALUES			64
#define MAX_EXPRESSION_OPERATORS		64

typedef struct expValue_s
{
	int		expressionRegister;

	int		brackets;
	int		parentheses;

	struct expValue_s	*prev;
	struct expValue_s	*next;
} expValue_t;

typedef struct expOperator_s
{
	opType_t		opType;
	int		priority;

	int		brackets;
	int		parentheses;

	struct expOperator_s *prev;
	struct expOperator_s *next;
} expOperator_t;

typedef struct
{
	int		numValues;
	expValue_t	values[MAX_EXPRESSION_VALUES];
	expValue_t	*firstValue;
	expValue_t	*lastValue;

	int		numOperators;
	expOperator_t	operators[MAX_EXPRESSION_OPERATORS];
	expOperator_t	*firstOperator;
	expOperator_t	*lastOperator;

	int		brackets;
	int		parentheses[MAX_EXPRESSION_VALUES + MAX_EXPRESSION_OPERATORS];

	int		resultRegister;
} shaderExpression_t;

static shaderExpression_t	r_shaderExpression;

static bool R_ParseExpressionValue( script_t *script, ref_shader_t *shader );
static bool R_ParseExpressionOperator( script_t *script, ref_shader_t *shader );

/*
=================
R_GetExpressionConstant
=================
*/
static bool R_GetExpressionConstant( float value, ref_shader_t *shader, int *expressionRegister )
{
	if( shader->numRegisters == MAX_EXPRESSION_REGISTERS )
	{
		MsgDev( D_WARN, "MAX_EXPRESSION_REGISTERS hit in shader '%s'\n", shader->name );
		return false;
	}

	shader->expressions[shader->numRegisters] = value;
	*expressionRegister = r_shaderExpression.resultRegister = shader->numRegisters++;

	return true;
}

/*
=================
R_GetExpressionTemporary
=================
*/
static bool R_GetExpressionTemporary( ref_shader_t *shader, int *expressionRegister )
{
	if( shader->numRegisters == MAX_EXPRESSION_REGISTERS )
	{
		MsgDev( D_WARN, "MAX_EXPRESSION_REGISTERS hit in shader '%s'\n", shader->name );
		return false;
	}

	shader->expressions[shader->numRegisters] = 0.0;
	*expressionRegister = r_shaderExpression.resultRegister = shader->numRegisters++;

	return true;
}

/*
=================
R_EmitExpressionOp
=================
*/
static bool R_EmitExpressionOp( opType_t opType, int a, int b, int c, ref_shader_t *shader )
{
	statement_t	*op;

	if( shader->numstatements == MAX_EXPRESSION_OPS )
	{
		MsgDev( D_WARN, "MAX_EXPRESSION_OPS hit in shader '%s'\n", shader->name );
		return false;
	}

	op = &shader->statements[shader->numstatements++];
	op->opType = opType;
	op->a = a;
	op->b = b;
	op->c = c;

	return true;
}

/*
=================
R_AddExpressionValue
=================
*/
static bool R_AddExpressionValue( int expressionRegister, ref_shader_t *shader )
{
	expValue_t	*v;

	if( r_shaderExpression.numValues == MAX_EXPRESSION_VALUES )
	{
		MsgDev( D_WARN, "MAX_EXPRESSION_VALUES hit for expression in material '%s'\n", shader->name );
		return false;
	}

	v = &r_shaderExpression.values[r_shaderExpression.numValues++];
	v->expressionRegister = expressionRegister;
	v->brackets = r_shaderExpression.brackets;
	v->parentheses = r_shaderExpression.parentheses[r_shaderExpression.brackets];
	v->next = NULL;
	v->prev = r_shaderExpression.lastValue;

	if (r_shaderExpression.lastValue)
		r_shaderExpression.lastValue->next = v;
	else
		r_shaderExpression.firstValue = v;

	r_shaderExpression.lastValue = v;

	return true;
}

/*
=================
R_AddExpressionOperator
=================
*/
static bool R_AddExpressionOperator (opType_t opType, int priority, ref_shader_t *shader){

	expOperator_t	*o;

	if (r_shaderExpression.numOperators == MAX_EXPRESSION_OPERATORS){
		MsgDev( D_WARN, "MAX_EXPRESSION_OPERATORS hit for expression in material '%s'\n", shader->name);
		return false;
	}

	o = &r_shaderExpression.operators[r_shaderExpression.numOperators++];

	o->opType = opType;
	o->priority = priority;
	o->brackets = r_shaderExpression.brackets;
	o->parentheses = r_shaderExpression.parentheses[r_shaderExpression.brackets];
	o->next = NULL;
	o->prev = r_shaderExpression.lastOperator;

	if (r_shaderExpression.lastOperator)
		r_shaderExpression.lastOperator->next = o;
	else
		r_shaderExpression.firstOperator = o;

	r_shaderExpression.lastOperator = o;

	return true;
}

/*
=================
R_ParseExpressionValue
=================
*/
static bool R_ParseExpressionValue( script_t *script, ref_shader_t *shader )
{
	token_t	token;
	table_t	*table;
	int	expressionRegister;

	// a newline separates commands
	if( !Com_ReadToken( script, 0, &token ))
	{
		MsgDev( D_WARN, "unexpected end of expression in material '%s'\n", shader->name );
		return false;
	}

	// a comma separates arguments
	if( !com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "unexpected end of expression in material '%s'\n", shader->name );
		return false;
	}

	// add a new value
	if( token.type != TT_NUMBER && token.type != TT_NAME && token.type != TT_PUNCTUATION )
	{
		MsgDev( D_WARN, "invalid value '%s' for expression in shader '%s'\n", token.string, shader->name );
		return false;
	}

	switch( token.type )
	{
	case TT_NUMBER:
		// it's a constant
		if( !R_GetExpressionConstant( token.floatValue, shader, &expressionRegister ))
			return false;
		if( !R_AddExpressionValue( expressionRegister, shader ))
			return false;
		break;
	case TT_NAME:
		// check for a table
		table = R_FindTable( token.string );
		if( table )
		{
			// the next token should be an opening bracket
			Com_ReadToken( script, 0, &token);
			if( com.stricmp( token.string, "[" ))
			{
				MsgDev( D_WARN, "expected '[', found '%s' instead for expression in shader '%s'\n", token.string, shader->name );
				return false;
			}

			r_shaderExpression.brackets++;

			if( !R_AddExpressionValue( table->index, shader ))
				return false;
			if( !R_AddExpressionOperator( OP_TYPE_TABLE, 0, shader ))
				return false;

			// we still expect a value
			return R_ParseExpressionValue( script, shader );
		}

		// check for a variable
		if( !com.stricmp( token.string, "glPrograms" ))
		{
			if( GL_Support( R_VERTEX_PROGRAM_EXT ) && GL_Support( R_FRAGMENT_PROGRAM_EXT ))
			{
				r_shaderExpression.resultRegister = EXP_REGISTER_ONE;

				if( !R_AddExpressionValue(EXP_REGISTER_ONE, shader ))
					return false;
			}
			else
			{
				r_shaderExpression.resultRegister = EXP_REGISTER_ZERO;

				if( !R_AddExpressionValue(EXP_REGISTER_ZERO, shader ))
					return false;
			}
		}
		else if( !com.stricmp( token.string, "time" ))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_TIME;

			if( !R_AddExpressionValue( EXP_REGISTER_TIME, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm0" ))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM0;

			if( !R_AddExpressionValue( EXP_REGISTER_PARM0, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm1" ))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM1;

			if( !R_AddExpressionValue( EXP_REGISTER_PARM1, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm2" ))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM2;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM2, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm3" ))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM3;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM3, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm4"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM4;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM4, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm5"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM5;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM5, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm6"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM6;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM6, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "parm7"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_PARM7;

			if( !R_AddExpressionValue(EXP_REGISTER_PARM7, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global0"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL0;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL0, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global1"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL1;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL1, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global2"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL2;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL2, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global3"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL3;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL3, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global4"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL4;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL4, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global5"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL5;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL5, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global6"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL6;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL6, shader ))
				return false;
		}
		else if( !com.stricmp( token.string, "global7"))
		{
			r_shaderExpression.resultRegister = EXP_REGISTER_GLOBAL7;

			if( !R_AddExpressionValue(EXP_REGISTER_GLOBAL7, shader ))
				return false;
		}
		else
		{
			MsgDev( D_WARN, "invalid value '%s' for expression in shader '%s'\n", token.string, shader->name );
			return false;
		}
		break;
	case TT_PUNCTUATION:
		// check for an opening parenthesis
		if( !com.stricmp( token.string, "("))
		{
			r_shaderExpression.parentheses[r_shaderExpression.brackets]++;

			// We still expect a value
			return R_ParseExpressionValue(script, shader);
		}

		// check for a minus operator before a constant
		if( !com.stricmp( token.string, "-"))
		{
			if( !Com_ReadToken( script, 0, &token))
			{
				MsgDev( D_WARN, "unexpected end of expression in shader '%s'\n", shader->name );
				return false;
			}
			if( token.type != TT_NUMBER )
			{
				MsgDev( D_WARN, "invalid value '%s' for expression in shader '%s'\n", token.string, shader->name );
				return false;
			}
			if( !R_GetExpressionConstant(-token.floatValue, shader, &expressionRegister ))
				return false;

			if( !R_AddExpressionValue(expressionRegister, shader ))
				return false;
		}
		else
		{
			MsgDev( D_WARN, "invalid value '%s' for expression in shader '%s'\n", token.string, shader->name );
			return false;
		}
		break;
	}

	// we now expect an operator
	return R_ParseExpressionOperator( script, shader );
}

/*
=================
R_ParseExpressionOperator
=================
*/
static bool R_ParseExpressionOperator( script_t *script, ref_shader_t *shader )
{
	token_t	token;

	// a newline separates commands
	if( !Com_ReadToken( script, 0, &token ))
	{
		if( r_shaderExpression.brackets )
		{
			MsgDev( D_WARN, "no matching ']' for expression in shader '%s'\n", shader->name );
			return false;
		}

		if( r_shaderExpression.parentheses[r_shaderExpression.brackets] )
		{
			MsgDev( D_WARN, "no matching ')' for expression in shader '%s'\n", shader->name );
			return false;
		}

		return true;
	}

	// a comma separates arguments
	if( !com.stricmp( token.string, "," ))
	{
		if( r_shaderExpression.brackets )
		{
			MsgDev( D_WARN, "no matching ']' for expression in shader '%s'\n", shader->name );
			return false;
		}
		if(r_shaderExpression.parentheses[r_shaderExpression.brackets] )
		{
			MsgDev( D_WARN, "no matching ')' for expression in shader '%s'\n", shader->name );
			return false;
		}

		// save the token, because we'll expect a comma later
		Com_SaveToken(script, &token);
		return true;
	}

	// add a new operator
	if( token.type != TT_PUNCTUATION )
	{
		MsgDev( D_WARN, "invalid operator '%s' for expression in shader '%s'\n", token.string, shader->name );
		return false;
	}

	// Check for a closing bracket
	if( !com.stricmp( token.string, "]" ))
	{
		if( r_shaderExpression.parentheses[r_shaderExpression.brackets] )
		{
			MsgDev( D_WARN, "no matching ')' for expression in shader '%s'\n", shader->name );
			return false;
		}

		r_shaderExpression.brackets--;
		if( r_shaderExpression.brackets < 0 )
		{
			MsgDev( D_WARN, "no matching '[' for expression in shader '%s'\n", shader->name );
			return false;
		}

		// we still expect an operator
		return R_ParseExpressionOperator( script, shader );
	}

	// check for a closing parenthesis
	if( !com.stricmp( token.string, ")" ))
	{
		r_shaderExpression.parentheses[r_shaderExpression.brackets]--;
		if( r_shaderExpression.parentheses[r_shaderExpression.brackets] < 0 )
		{
			MsgDev( D_WARN, "no matching '(' for expression in shader '%s'\n", shader->name );
			return false;
		}

		// we still expect an operator
		return R_ParseExpressionOperator( script, shader );
	}

	// Check for an operator
	if( !com.stricmp( token.string, "*" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_MULTIPLY, 7, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "/" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_DIVIDE, 7, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "%" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_MOD, 6, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "+" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_ADD, 5, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "-")){
		if( !R_AddExpressionOperator(OP_TYPE_SUBTRACT, 5, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, ">" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_GREATER, 4, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "<" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_LESS, 4, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, ">=" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_GEQUAL, 4, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "<=" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_LEQUAL, 4, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "==" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_EQUAL, 3, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "!=" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_NOTEQUAL, 3, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "&&" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_AND, 2, shader ))
			return false;
	}
	else if( !com.stricmp( token.string, "||" ))
	{
		if( !R_AddExpressionOperator(OP_TYPE_OR, 1, shader ))
			return false;
	}
	else
	{
		MsgDev( D_WARN, "invalid operator '%s' for expression in shader '%s'\n", token.string, shader->name );
		return false;
	}

	// we now expect a value
	return R_ParseExpressionValue( script, shader );
}

/*
=================
R_ParseExpression
=================
*/
static bool R_ParseExpression( script_t *script, ref_shader_t *shader, int *expressionRegister )
{
	expValue_t	*v;
	expOperator_t	*o;
	int		a, b, c;

	// clear the previous expression data
	Mem_Set( &r_shaderExpression, 0, sizeof( shaderExpression_t ));

	// parse the expression, starting with a value
	if( !R_ParseExpressionValue( script, shader ))
		return false;

	// emit the expression ops, if any
	while( r_shaderExpression.firstOperator )
	{
		v = r_shaderExpression.firstValue;

		for( o = r_shaderExpression.firstOperator; o->next; o = o->next )
		{
			// if the current operator is nested deeper in brackets than
			// the next operator
			if( o->brackets > o->next->brackets )
				break;

			// if the current and next operators are nested equally deep
			// in brackets
			if( o->brackets == o->next->brackets )
			{
				// if the current operator is nested deeper in
				// parentheses than the next operator
				if( o->parentheses > o->next->parentheses )
					break;

				// if the current and next operators are nested equally
				// deep in parentheses
				if( o->parentheses == o->next->parentheses )
				{
					// if the priority of the current operator is equal
					// or higher than the priority of the next operator
					if( o->priority >= o->next->priority )
						break;
				}
			}
			v = v->next;
		}

		// get the source registers
		a = v->expressionRegister;
		b = v->next->expressionRegister;

		// get the temporary register
		if( !R_GetExpressionTemporary( shader, &c ))
			return false;

		// emit the expression op
		if( !R_EmitExpressionOp( o->opType, a, b, c, shader ))
			return false;

		// the temporary register for the current operation will be used
		// as a source for the next operation
		v->expressionRegister = c;

		// remove the second value
		v = v->next;

		if( v->prev ) v->prev->next = v->next;
		else r_shaderExpression.firstValue = v->next;

		if( v->next ) v->next->prev = v->prev;
		else r_shaderExpression.lastValue = v->prev;

		// remove the operator
		if( o->prev ) o->prev->next = o->next;
		else r_shaderExpression.firstOperator = o->next;

		if( o->next ) o->next->prev = o->prev;
		else r_shaderExpression.lastOperator = o->prev;
	}

	// the last temporary register will contain the result after evaluation
	*expressionRegister = r_shaderExpression.resultRegister;
	return true;
}

/*
=================
R_ParseWaveFunc
=================
*/
static bool R_ParseWaveFunc( ref_shader_t *shader, waveFunc_t *func, script_t *script )
{
	token_t	tok;
	int	i;

	if(!Com_ReadToken( script, false, &tok ))
		return false;

	if( !com.stricmp( tok.string, "sin" )) func->type = WAVEFORM_SIN;
	else if( !com.stricmp( tok.string, "triangle" )) func->type = WAVEFORM_TRIANGLE;
	else if( !com.stricmp( tok.string, "square" )) func->type = WAVEFORM_SQUARE;
	else if( !com.stricmp( tok.string, "sawtooth" )) func->type = WAVEFORM_SAWTOOTH;
	else if( !com.stricmp( tok.string, "inverseSawtooth" )) func->type = WAVEFORM_INVERSESAWTOOTH;
	else if( !com.stricmp( tok.string, "noise" )) func->type = WAVEFORM_NOISE;
	else
	{
		MsgDev( D_WARN, "unknown waveform '%s' in shader '%s', defaulting to sin\n", tok.string, shader->name );
		func->type = WAVEFORM_SIN;
	}

	for( i = 0; i < 4; i++ )
	{
		if( !Com_ReadFloat( script, false, &func->params[i] ))
			return false;
	}
	return true;
}

/*
=======================================================================

 SHADER PARSING

=======================================================================
*/
/*
=================
R_ParseGeneralSurfaceParm
=================
*/
static bool R_ParseGeneralSurfaceParm( ref_shader_t *shader, script_t *script )
{
	token_t	tok;
	int	i, numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);

	switch( shader->type )
	{
	case SHADER_TEXTURE:
	case SHADER_SKY: break;
	default:
		MsgDev( D_WARN, "'surfaceParm' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'surfaceParm' in shader '%s'\n", shader->name );
		return false;
	}

	for( i = 0; i < numInfoParms; i++ )
	{
		if( !com.stricmp( tok.string, infoParms[i].name ))
		{
			shader->surfaceParm |= infoParms[i].surfaceFlags;
			break;
		}
	}

	if(!(shader->surfaceParm & SURF_SKY) && shader->type == SHADER_SKY )
	{
		MsgDev( D_WARN, "invalid 'surfaceParm' for shader '%s'\n", shader->name );
		return false;
	}

	if( i == numInfoParms )
	{
		MsgDev( D_WARN, "unknown 'surfaceParm' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}

	shader->flags |= SHADER_SURFACEPARM;
	return true;
}

/*
=================
R_ParseGeneralIf
=================
*/
static bool R_ParseGeneralIf( script_t *script, ref_shader_t *shader )
{
	if( !R_ParseExpression( script, shader, &shader->conditionRegister ))
	{
		MsgDev( D_WARN, "missing expression parameters for 'if' in shader '%s'\n", shader->name );
		return false;
	}
	return true;
}

/*
=================
R_ParseGeneralNoPicmip
=================
*/
static bool R_ParseGeneralNoPicmip( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texFlags |= TF_NOPICMIP;
	return true;
}

/*
=================
R_ParseGeneralNoCompress
=================
*/
static bool R_ParseGeneralNoCompress( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texFlags |= TF_UNCOMPRESSED;
	return true;
}

/*
=================
R_ParseGeneralHighQuality
=================
*/
static bool R_ParseGeneralHighQuality( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texFlags |= (TF_NOPICMIP|TF_UNCOMPRESSED);
	return true;
}

/*
=================
R_ParseGeneralLinear
=================
*/
static bool R_ParseGeneralLinear( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texFilter = TF_LINEAR;
	return true;
}

/*
=================
R_ParseGeneralNearest
=================
*/
static bool R_ParseGeneralNearest( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texFilter = TF_NEAREST;
	return true;
}

/*
=================
R_ParseGeneralClamp
=================
*/
static bool R_ParseGeneralClamp( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texWrap = TW_CLAMP;
	return true;
}

/*
=================
R_ParseGeneralZeroClamp
=================
*/
static bool R_ParseGeneralZeroClamp( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texWrap = TW_CLAMP_TO_ZERO;
	return true;
}

/*
=================
R_ParseGeneralAlphaZeroClamp
=================
*/
static bool R_ParseGeneralAlphaZeroClamp( ref_shader_t *shader, script_t *script )
{
	int	i, j;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
			shader->stages[i]->bundles[j]->texWrap = TW_CLAMP_TO_ZERO_ALPHA;
	return true;
}

/*
=================
R_ParseGeneralMirror
=================
*/
static bool R_ParseGeneralMirror( ref_shader_t *shader, script_t *script )
{
	if( shader->type != SHADER_TEXTURE )
	{
		MsgDev( D_WARN, "'mirror' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( shader->subview != SUBVIEW_NONE && shader->subview != SUBVIEW_MIRROR )
	{
		MsgDev( D_WARN, "multiple subview types for shader '%s'\n", shader->name );
		return false;
	}

	shader->sort = SORT_SUBVIEW;
	shader->subview = SUBVIEW_MIRROR;
	shader->subviewWidth = 0;
	shader->subviewHeight = 0;
	return true;
}

/*
=================
R_ParseGeneralSort
=================
*/
static bool R_ParseGeneralSort( ref_shader_t *shader, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'sort' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "subview" )) shader->sort = SORT_SUBVIEW;
	else if( !com.stricmp( tok.string, "opaque" )) shader->sort = SORT_OPAQUE;
	else if( !com.stricmp( tok.string, "sky" )) shader->sort = SORT_SKY;
	else if( !com.stricmp( tok.string, "decal" )) shader->sort = SORT_DECAL;
	else if( !com.stricmp( tok.string, "seeThrough" )) shader->sort = SORT_SEETHROUGH;
	else if( !com.stricmp( tok.string, "banner" )) shader->sort = SORT_BANNER;
	else if( !com.stricmp( tok.string, "underwater" )) shader->sort = SORT_UNDERWATER;
	else if( !com.stricmp( tok.string, "water" )) shader->sort = SORT_WATER;
	else if( !com.stricmp( tok.string, "farthest" )) shader->sort = SORT_FARTHEST;
	else if( !com.stricmp( tok.string, "far" )) shader->sort = SORT_FAR;
	else if( !com.stricmp( tok.string, "medium" )) shader->sort = SORT_MEDIUM;
	else if( !com.stricmp( tok.string, "close" )) shader->sort = SORT_CLOSE;
	else if( !com.stricmp( tok.string, "additive" )) shader->sort = SORT_ADDITIVE;
	else if( !com.stricmp( tok.string, "almostNearest" )) shader->sort = SORT_ALMOST_NEAREST;
	else if( !com.stricmp( tok.string, "nearest" )) shader->sort = SORT_NEAREST;
	else if( !com.stricmp( tok.string, "postProcess" )) shader->sort = SORT_POST_PROCESS;
	else
	{
		shader->sort = com.atoi( tok.string );

		if( shader->sort < 1 || shader->sort > 15 )
		{
			MsgDev( D_WARN, "unknown 'sort' parameter '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
	}
	shader->flags |= SHADER_SORT;
	return true;
}

/*
=================
R_ParseGeneralCull
=================
*/
static bool R_ParseGeneralCull( ref_shader_t *shader, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok )) shader->cull.mode = GL_FRONT;
	else
	{
		if( !com.stricmp( tok.string, "front")) shader->cull.mode = GL_FRONT;
		else if( !com.stricmp( tok.string, "back" ) || !com.stricmp( tok.string, "backSide" ) || !com.stricmp( tok.string, "backSided" ))
			shader->cull.mode = GL_BACK;
		else if( !com.stricmp( tok.string, "disable" ) || !com.stricmp( tok.string, "none" ) ||  !com.stricmp( tok.string, "twoSided" ))
			shader->cull.mode = 0;
		else
		{
			MsgDev( D_WARN, "unknown 'cull' parameter '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
	}
	shader->flags |= SHADER_CULL;
	return true;
}

/*
=================
R_ParseGeneralPolygonOffset
=================
*/
static bool R_ParseGeneralPolygonOffset( ref_shader_t *shader, script_t *script )
{
	shader->flags |= SHADER_POLYGONOFFSET;
	return true;
}

/*
=================
R_ParseGeneralDeformVertexes
=================
*/
static bool R_ParseGeneralDeformVertexes( ref_shader_t *shader, script_t *script )
{
	deform_t	*deformVertexes;
	token_t	tok;
	int	i;

	if( shader->numDeforms == SHADER_MAX_TRANSFORMS )
	{
		MsgDev( D_WARN, "SHADER_MAX_TRANSFORMS hit in shader '%s'\n", shader->name );
		return false;
	}
	deformVertexes = &shader->deform[shader->numDeforms++];

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'deformVertexes' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "wave" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_WARN, "missing parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}

		deformVertexes->params[0] = com.atof( tok.string );
		if( deformVertexes->params[0] == 0.0 )
		{
			MsgDev( D_WARN, "illegal div value of 0 for 'deformVertexes wave' in shader '%s', defaulting to 100\n", shader->name );
			deformVertexes->params[0] = 100.0;
		}
		deformVertexes->params[0] = 1.0 / deformVertexes->params[0];

		if(!R_ParseWaveFunc( shader, &deformVertexes->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}
		deformVertexes->type = DEFORM_WAVE;
	}
	else if( !com.stricmp( tok.string, "move" ))
	{
		for( i = 0; i < 3; i++ )
		{
			if( !Com_ReadFloat( script, false, &deformVertexes->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
				return false;
			}
		}

		if(!R_ParseWaveFunc( shader, &deformVertexes->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
			return false;
		}
		deformVertexes->type = DEFORM_MOVE;
	}
	else if( !com.stricmp( tok.string, "normal" ))
	{
		for( i = 0; i < 2; i++ )
		{
			if( !Com_ReadFloat( script, false, &deformVertexes->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'deformVertexes normal' in shader '%s'\n", shader->name );
				return false;
			}
		}
		deformVertexes->type = DEFORM_NORMAL;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'deformVertexes' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}

	shader->flags |= SHADER_DEFORMVERTEXES;
	return true;
}

/*
=================
R_ParseGeneralNoShadows
=================
*/
static bool R_ParseGeneralNoShadows( ref_shader_t *shader, script_t *script )
{
	shader->flags |= SHADER_NOSHADOWS;
	return true;
}

/*
=================
R_ParseGeneralNoFragments
=================
*/
static bool R_ParseGeneralNoFragments( ref_shader_t *shader, script_t *script )
{
	shader->flags |= SHADER_NOFRAGMENTS;
	return true;
}

/*
=================
R_ParseGeneralEntityMergable
=================
*/
static bool R_ParseGeneralEntityMergable( ref_shader_t *shader, script_t *script )
{
	shader->flags |= SHADER_ENTITYMERGABLE;
	return true;
}

/*
=================
R_ParseGeneralTessSize
=================
*/
static bool R_ParseGeneralTessSize( ref_shader_t *shader, script_t *script )
{
	token_t	tok;
	int	i = 8;

	if( shader->type != SHADER_TEXTURE )
	{
		MsgDev( D_WARN, "'tessSize' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'tessSize' in shader '%s'\n", shader->name );
		return false;
	}

	shader->tessSize = com.atoi( tok.string );

	if( shader->tessSize < 8 || shader->tessSize > 256 )
	{
		MsgDev( D_WARN, "out of range size value of %i for 'tessSize' in shader '%s', defaulting to 64\n", shader->tessSize, shader->name );
		shader->tessSize = 64;
	}
	else
	{
		while( i <= shader->tessSize ) i<<=1;
		shader->tessSize = i>>1;
	}

	shader->flags |= SHADER_TESSSIZE;
	return true;
}

/*
=================
R_ParseGeneralSkyParms
=================
*/
static bool R_ParseGeneralSkyParms( ref_shader_t *shader, script_t *script )
{
	string	name;
	token_t	tok;
	int	i;

	if( shader->type != SHADER_SKY )
	{
		MsgDev( D_WARN, "'skyParms' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( com.stricmp( tok.string, "-" ))
	{
		for( i = 0; i < 6; i++ )
		{
			com.snprintf( name, sizeof( name ), "%s%s", tok.string, r_skyBoxSuffix[i] );
			shader->skyParms.farBox[i] = R_FindTexture( name, NULL, 0, 0, TF_LINEAR, TW_CLAMP );
			if( !shader->skyParms.farBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", name, shader->name );
				return false;
			}
		}
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( com.stricmp( tok.string, "-" ))
	{
		shader->skyParms.cloudHeight = com.atof( tok.string );
		if( shader->skyParms.cloudHeight < 8.0 || shader->skyParms.cloudHeight > 1024.0 )
		{
			MsgDev( D_WARN, "out of range cloudHeight value of %f for 'skyParms' in shader '%s', defaulting to 128\n", shader->skyParms.cloudHeight, shader->name );
			shader->skyParms.cloudHeight = 128.0;
		}
	}
	else shader->skyParms.cloudHeight = 128.0;

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( com.stricmp( tok.string, "-" ))
	{
		for( i = 0; i < 6; i++ )
		{
			com.snprintf( name, sizeof(name), "%s%s", tok.string, r_skyBoxSuffix[i] );
			shader->skyParms.nearBox[i] = R_FindTexture( name, NULL, 0, 0, TF_LINEAR, TW_CLAMP );
			if( !shader->skyParms.nearBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", name, shader->name );
				return false;
			}
		}
	}
	shader->flags |= SHADER_SKYPARMS;
	return true;
}

/*
=================
R_ParseStageIf
=================
*/
static bool R_ParseStageIf( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	if( !R_ParseExpression( script, shader, &stage->conditionRegister ))
	{
		MsgDev( D_WARN, "missing expression parameters for 'if' in shader '%s'\n", shader->name );
		return false;
	}
	return true;
}
/*
=================
R_ParseStageNoPicMip
=================
*/
static bool R_ParseStageNoPicMip( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texFlags |= TF_NOPICMIP;

	return true;
}

/*
=================
R_ParseStageNoCompress
=================
*/
static bool R_ParseStageNoCompress( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texFlags |= TF_UNCOMPRESSED;

	return true;
}

/*
=================
R_ParseStageHighQuality
=================
*/
static bool R_ParseStageHighQuality( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texFlags |= (TF_NOPICMIP|TF_UNCOMPRESSED);

	return true;
}

/*
=================
R_ParseStageNearest
=================
*/
static bool R_ParseStageNearest( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texFilter = TF_NEAREST;

	return true;
}

/*
=================
R_ParseStageLinear
=================
*/
static bool R_ParseStageLinear ( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texFilter = TF_LINEAR;

	return true;
}

/*
=================
R_ParseStageNoClamp
=================
*/
static bool R_ParseStageNoClamp( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texWrap = TW_REPEAT;

	return true;
}

/*
=================
R_ParseStageClamp
=================
*/
static bool R_ParseStageClamp( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texWrap = TW_CLAMP;

	return true;
}

/*
=================
R_ParseStageZeroClamp
=================
*/
static bool R_ParseStageZeroClamp( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texWrap = TW_CLAMP_TO_ZERO;

	return true;
}

/*
=================
R_ParseStageAlphaZeroClamp
=================
*/
static bool R_ParseStageAlphaZeroClamp( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->texWrap = TW_CLAMP_TO_ZERO_ALPHA;

	return true;
}

/*
=================
R_ParseStageAnimFrequency
=================
*/
static bool R_ParseStageAnimFrequency( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];

	if( !Com_ReadFloat( script, false, &bundle->animFrequency ))
	{
		MsgDev( D_WARN, "missing parameters for 'animFrequency' in shader '%s\n", shader->name );
		return false;
	}

	bundle->flags |= STAGEBUNDLE_ANIMFREQUENCY;
	return true;
}

/*
=================
R_ParseStageMap
=================
*/
static bool R_ParseStageMap( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	string		name;
	token_t		tok;

	if( shader->type == SHADER_NOMIP )
		bundle->texFlags |= TF_NOPICMIP;

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_MAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'map' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'map' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "$lightmap"))
	{
		if( shader->type != SHADER_TEXTURE )
		{
			MsgDev( D_WARN, "'map $lightmap' not allowed in shader '%s'\n", shader->name );
			return false;
		}

		if( bundle->flags & STAGEBUNDLE_ANIMFREQUENCY )
		{
			MsgDev( D_WARN, "'map $lightmap' not allowed with 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}

		bundle->texType = TEX_LIGHTMAP;
		bundle->flags |= STAGEBUNDLE_MAP;
		shader->flags |= SHADER_HASLIGHTMAP;

		return true;
	}

	com.strncpy( name, tok.string, sizeof( name ));
	if( !com.stricmp( tok.string, "$whiteImage" ))
		bundle->textures[bundle->numTextures++] = r_whiteTexture;
	else if( !com.stricmp( tok.string, "$blackImage"))
		bundle->textures[bundle->numTextures++] = r_blackTexture;
	else if( !com.stricmp( tok.string, "$internal"))
		bundle->textures[bundle->numTextures++] = r_internalMiptex;
	else
	{
		while( 1 )
		{
			if( !Com_ReadToken( script, SC_PARSE_GENERIC, &tok ))
				break;

			com.strncat( name, " ", sizeof( name ));
			com.strncat( name, tok.string, sizeof( name ));
		}
		bundle->textures[bundle->numTextures] = R_FindTexture( name, NULL, 0, bundle->texFlags, bundle->texFilter, bundle->texWrap );
		if( !bundle->textures[bundle->numTextures] )
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_MAP;

	return true;
}

/*
=================
R_ParseStageBumpMap
=================
*/
static bool R_ParseStageBumpMap( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	string		name;
	token_t		tok;

	if( shader->type == SHADER_NOMIP )
		bundle->texFlags |= TF_NOPICMIP;

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_BUMPMAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'bumpMap' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'bumpMap' in shader '%s'\n", shader->name );
		return false;
	}

	com.strncpy( name, tok.string, sizeof( name ));
	while( 1 )
	{
		if( !Com_ReadToken( script, SC_PARSE_GENERIC, &tok ))
			break;

		com.strncat( name, " ", sizeof( name ));
		com.strncat( name, tok.string, sizeof( name ));
	}

	bundle->texFlags |= TF_NORMALMAP;
	bundle->textures[bundle->numTextures] = R_FindTexture( name, NULL, 0, bundle->texFlags, bundle->texFilter, bundle->texWrap );
	if( !bundle->textures[bundle->numTextures] )
	{
		MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}

	bundle->numTextures++;
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_BUMPMAP;

	return true;
}

/*
=================
R_ParseStageCubeMap
=================
*/
static bool R_ParseStageCubeMap( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	token_t		tok;

	if( shader->type == SHADER_NOMIP )
		bundle->texFlags |= TF_NOPICMIP;

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'cubeMap' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
		return false;
	}

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_CUBEMAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'cubeMap' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'cubeMap' in shader '%s'\n", shader->name );
		return false;
	}

	bundle->texFlags |= TF_CUBEMAP;

	if( !com.stricmp( tok.string, "$normalize" ))
	{
		if( bundle->flags & STAGEBUNDLE_ANIMFREQUENCY )
		{
			MsgDev( D_WARN, "'cubeMap $normalize' not allowed with 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
		bundle->textures[bundle->numTextures++] = r_normalizeTexture;
	}
	else
	{
		bundle->textures[bundle->numTextures] = R_FindCubeMapTexture( tok.string, NULL, 0, bundle->texFlags, bundle->texFilter, bundle->texWrap );
		if( !bundle->textures[bundle->numTextures] )
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_CUBEMAP;

	return true;
}

/*
=================
R_ParseStageVideoMap
=================
*/
static bool R_ParseStageVideoMap( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	token_t		tok;

	if( bundle->numTextures )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s\n", shader->name );
		return false;
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "multiple 'videoMap' specifications in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'videoMap' in shader '%s'\n", shader->name );
		return false;
	}

	//FIXME: implement
	//bundle->cinematicHandle = R_PlayVideo( tok.string );
	if( !bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "couldn't find video '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	bundle->texType = TEX_CINEMATIC;
	bundle->flags |= STAGEBUNDLE_VIDEOMAP;

	return true;
}

/*
=================
R_ParseStageTexEnvCombine
=================
*/
static bool R_ParseStageTexEnvCombine( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	int		numArgs;
	token_t		tok;
	int		i;

	if(!GL_Support( R_COMBINE_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'texEnvCombine' without 'requires GL_ARB_texture_env_combine'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		if(!(bundle->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			MsgDev( D_WARN, "shader '%s' uses 'texEnvCombine' in a bundle without 'nextBundle combine'\n", shader->name );
			return false;
		}
	}

	// setup default params
	bundle->texEnv = GL_COMBINE_ARB;
	bundle->texEnvCombine.rgbCombine = GL_MODULATE;
	bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
	bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
	bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
	bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
	bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
	bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
	bundle->texEnvCombine.rgbScale = 1;
	bundle->texEnvCombine.alphaCombine = GL_MODULATE;
	bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
	bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
	bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
	bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaScale = 1;
	bundle->texEnvCombine.constColor[0] = 1.0;
	bundle->texEnvCombine.constColor[1] = 1.0;
	bundle->texEnvCombine.constColor[2] = 1.0;
	bundle->texEnvCombine.constColor[3] = 1.0;

	if( !Com_ReadToken( script, true, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'texEnvCombine' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "{"))
	{
		while( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
			{
				MsgDev( D_WARN, "no concluding '}' in 'texEnvCombine' in shader '%s'\n", shader->name );
				return false;
			}

			if( !com.stricmp( tok.string, "}")) break;

			if( !com.stricmp( tok.string, "rgb"))
			{
				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "="))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				if( !Com_ReadToken( script, false, &tok ))
				{
					MsgDev( D_WARN, "missing 'rgb' equation name for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}

				if( !com.stricmp( tok.string, "REPLACE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_REPLACE;
					numArgs = 1;
				}
				else if( !com.stricmp( tok.string, "MODULATE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_MODULATE;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "ADD"))
				{
					bundle->texEnvCombine.rgbCombine = GL_ADD;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "ADD_SIGNED"))
				{
					bundle->texEnvCombine.rgbCombine = GL_ADD_SIGNED_ARB;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "INTERPOLATE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_INTERPOLATE_ARB;
					numArgs = 3;
				}
				else if( !com.stricmp( tok.string, "SUBTRACT"))
				{
					bundle->texEnvCombine.rgbCombine = GL_SUBTRACT_ARB;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "DOT3_RGB"))
				{
					if(!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGB' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbCombine = GL_DOT3_RGB_ARB;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "DOT3_RGBA"))
				{
					if(!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGBA' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbCombine = GL_DOT3_RGBA_ARB;
					numArgs = 2;
				}
				else
				{
					MsgDev( D_WARN, "unknown 'rgb' equation name '%s' for 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "(" ))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				for( i = 0; i < numArgs; i++ )
				{
					if( !Com_ReadToken( script, false, &tok ))
					{
						MsgDev( D_WARN, "missing 'rgb' equation arguments for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}

					if( !com.stricmp( tok.string, "Ct"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "1-Ct"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "At"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-At"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Cc"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "1-Cc"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "Ac"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Ac"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Cf"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "1-Cf"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "Af"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Af"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Cp"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "1-Cp"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if( !com.stricmp( tok.string, "Ap"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Ap"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else
					{
						MsgDev( D_WARN, "unknown 'rgb' equation argument '%s' for 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
						return false;
					}

					if( i < numArgs - 1 )
					{
						Com_ReadToken( script, false, &tok );
						if( com.stricmp( tok.string, "," ))
						{
							MsgDev( D_WARN, "expected ',', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
							return false;
						}
					}
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, ")" ))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				if( Com_ReadToken( script, false, &tok ))
				{
					if( com.stricmp( tok.string, "*"))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
						return false;
					}

					if( !Com_ReadToken( script, false, &tok ))
					{
						MsgDev( D_WARN, "missing scale value for 'texEnvCombine' equation in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbScale = com.atoi( tok.string );

					if( bundle->texEnvCombine.rgbScale != 1 && bundle->texEnvCombine.rgbScale != 2 && bundle->texEnvCombine.rgbScale != 4 )
					{
						MsgDev( D_WARN, "invalid scale value of %i for 'texEnvCombine' equation in shader '%s'\n", bundle->texEnvCombine.rgbScale, shader->name );
						return false;
					}
				}
			}
			else if( !com.stricmp( tok.string, "alpha"))
			{
				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "=" ))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				if( !Com_ReadToken( script, false, &tok ))
				{
					MsgDev( D_WARN, "missing 'alpha' equation name for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}

				if( !com.stricmp( tok.string, "REPLACE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_REPLACE;
					numArgs = 1;
				}
				else if( !com.stricmp( tok.string, "MODULATE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_MODULATE;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "ADD"))
				{
					bundle->texEnvCombine.alphaCombine = GL_ADD;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "ADD_SIGNED"))
				{
					bundle->texEnvCombine.alphaCombine = GL_ADD_SIGNED_ARB;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "INTERPOLATE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_INTERPOLATE_ARB;
					numArgs = 3;
				}
				else if( !com.stricmp( tok.string, "SUBTRACT"))
				{
					bundle->texEnvCombine.alphaCombine = GL_SUBTRACT_ARB;
					numArgs = 2;
				}
				else if( !com.stricmp( tok.string, "DOT3_RGB"))
				{
					if( !GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGB' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					MsgDev( D_WARN, "'DOT3_RGB' is not a valid 'alpha' equation for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}
				else if( !com.stricmp( tok.string, "DOT3_RGBA"))
				{
					if( !GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGBA' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					MsgDev( D_WARN, "'DOT3_RGBA' is not a valid 'alpha' equation for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}
				else
				{
					MsgDev( D_WARN, "unknown 'alpha' equation name '%s' for 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "(" ))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				for( i = 0; i < numArgs; i++ )
				{
					if( !Com_ReadToken( script, false, &tok ))
					{
						MsgDev( D_WARN, "missing 'alpha' equation arguments for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}

					if( !com.stricmp( tok.string, "At"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-At"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Ac"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Ac"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Af"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Af"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "Ap"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if( !com.stricmp( tok.string, "1-Ap"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else
					{
						MsgDev( D_WARN, "unknown 'alpha' equation argument '%s' for 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
						return false;
					}

					if( i < numArgs - 1 )
					{
						Com_ReadToken( script, false, &tok );
						if( com.stricmp( tok.string, "," ))
						{
							MsgDev( D_WARN, "expected ',', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
							return false;
						}
					}
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, ")" ))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				if( Com_ReadToken( script, false, &tok ))
				{
					if( com.stricmp( tok.string, "*" ))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
						return false;
					}

					if( !Com_ReadToken( script, false, &tok ))
					{
						MsgDev( D_WARN, "missing scale value for 'texEnvCombine' equation in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.alphaScale = com.atoi( tok.string );

					if( bundle->texEnvCombine.alphaScale != 1 && bundle->texEnvCombine.alphaScale != 2 && bundle->texEnvCombine.alphaScale != 4 )
					{
						MsgDev( D_WARN, "invalid scale value of %i for 'texEnvCombine' equation in shader '%s'\n", bundle->texEnvCombine.alphaScale, shader->name );
						bundle->texEnvCombine.alphaScale = 1;
					}
				}
			}
			else if( !com.stricmp( tok.string, "const"))
			{
				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "=" ))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, "(" ))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				for( i = 0; i < 4; i++ )
				{
					if( !Com_ReadToken( script, false, &tok ))
					{
						MsgDev( D_WARN, "missing 'const' color value for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.constColor[i] = bound( 0.0, com.atof( tok.string ), 1.0 );
				}

				Com_ReadToken( script, false, &tok );
				if( com.stricmp( tok.string, ")" ))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
					return false;
				}

				if( Com_ReadToken( script, false, &tok ))
				{
					if( com.stricmp( tok.string, "*" ))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
						return false;
					}

					Com_ReadToken( script, false, &tok );
					if( com.stricmp( tok.string, "identityLighting" ))
					{
						MsgDev( D_WARN, "'const' color for 'texEnvCombine' can only be scaled by 'identityLighting' in shader '%s'\n", shader->name );
						return false;
					}

					if( gl_config.deviceSupportsGamma )
					{
						for( i = 0; i < 3; i++ )
							bundle->texEnvCombine.constColor[i] *= (1.0 / (float)(1<<r_overbrightbits->integer));
					}
				}
			}
			else
			{
				MsgDev( D_WARN, "unknown 'texEnvCombine' parameter '%s' in shader '%s'\n", tok.string, shader->name );
				return false;
			}
		}
	}
	else
	{
		MsgDev( D_WARN, "expected '{', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;

	return true;
}

/*
=================
R_ParseStageTcGen
=================
*/
static bool R_ParseStageTcGen( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	token_t		tok;
	int		i, j;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'tcGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "base" ) || !com.stricmp( tok.string, "texture" ))
		bundle->tcGen.type = TCGEN_BASE;
	else if( !com.stricmp( tok.string, "lightmap" ))
		bundle->tcGen.type = TCGEN_LIGHTMAP;
	else if( !com.stricmp( tok.string, "environment" ))
		bundle->tcGen.type = TCGEN_ENVIRONMENT;
	else if( !com.stricmp( tok.string, "vector" ))
	{
		for( i = 0; i < 2; i++ )
		{
			Com_ReadToken( script, false, &tok );
			if( com.stricmp( tok.string, "(" ))
			{
				MsgDev( D_WARN, "missing '(' for 'tcGen vector' in shader '%s'\n", shader->name );
				return false;
			}

			for( j = 0; j < 3; j++ )
			{
				if( !Com_ReadToken( script, false, &tok ))
				{
					MsgDev( D_WARN, "missing parameters for 'tcGen vector' in shader '%s'\n", shader->name );
					return false;
				}
				bundle->tcGen.params[i*3+j] = com.atof( tok.string );
			}

			Com_ReadToken( script, false, &tok );
			if( com.stricmp( tok.string, ")" ))
			{
				MsgDev( D_WARN, "missing ')' for 'tcGen vector' in shader '%s'\n", shader->name );
				return false;
			}
		}
		bundle->tcGen.type = TCGEN_VECTOR;
	}
	else if( !com.stricmp( tok.string, "warp" )) bundle->tcGen.type = TCGEN_WARP;
	else if( !com.stricmp( tok.string, "lightVector" )) bundle->tcGen.type = TCGEN_LIGHTVECTOR;
	else if( !com.stricmp( tok.string, "halfAngle" )) bundle->tcGen.type = TCGEN_HALFANGLE;
	else if( !com.stricmp( tok.string, "reflection" ))
	{
		if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		{
			MsgDev( D_WARN, "shader '%s' uses 'tcGen reflection' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
			return false;
		}
		bundle->tcGen.type = TCGEN_REFLECTION;
	}
	else if( !com.stricmp( tok.string, "normal"))
	{
		if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		{
			MsgDev( D_WARN, "shader '%s' uses 'tcGen normal' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
			return false;
		}
		bundle->tcGen.type = TCGEN_NORMAL;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'tcGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TCGEN;
	return true;
}

/*
=================
R_ParseStageTcMod
=================
*/
static bool R_ParseStageTcMod( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	tcMod_t		*tcMod;
	token_t		tok;
	int		i;

	if( bundle->tcModNum == SHADER_MAX_TCMOD )
	{
		MsgDev( D_WARN, "SHADER_MAX_TCMOD hit in shader '%s'\n", shader->name );
		return false;
	}
	tcMod = &bundle->tcMod[bundle->tcModNum++];

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'tcMod' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "translate" ))
	{
		for( i = 0; i < 2; i++ )
		{
			if( !Com_ReadFloat( script, false, &tcMod->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod translate' in shader '%s'\n", shader->name );
				return false;
			}
		}
		tcMod->type = TCMOD_TRANSLATE;
	}
	else if( !com.stricmp( tok.string, "scale" ))
	{
		for( i = 0; i < 2; i++ )
		{
			if( !Com_ReadFloat( script, false, &tcMod->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod scale' in shader '%s'\n", shader->name );
				return false;
			}
		}
		tcMod->type = TCMOD_SCALE;
	}
	else if( !com.stricmp( tok.string, "scroll" ))
	{
		for( i = 0; i < 2; i++ )
		{
			if( !Com_ReadFloat( script, false, &tcMod->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod scroll' in shader '%s'\n", shader->name );
				return false;
			}
		}
		tcMod->type = TCMOD_SCROLL;
	}
	else if( !com.stricmp( tok.string, "rotate" ))
	{
		if( !Com_ReadFloat( script, false, &tcMod->params[0] ))
		{
			MsgDev( D_WARN, "missing parameters for 'tcMod rotate' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_ROTATE;
	}
	else if( !com.stricmp( tok.string, "stretch" ))
	{
		if(!R_ParseWaveFunc( shader, &tcMod->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'tcMod stretch' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_STRETCH;
	}
	else if( !com.stricmp( tok.string, "turb" ))
	{
		tcMod->func.type = WAVEFORM_SIN;

		for( i = 0; i < 4; i++ )
		{
			if( !Com_ReadFloat( script, false, &tcMod->func.params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod turb' in shader '%s'\n", shader->name );
				return false;
			}
		}
		tcMod->type = TCMOD_TURB;
	}
	else if( !com.stricmp( tok.string, "transform" ))
	{
		for( i = 0; i < 6; i++ )
		{
			if( !Com_ReadFloat( script, false, &tcMod->params[i] ))
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod transform' in shader '%s'\n", shader->name );
				return false;
			}
		}
		tcMod->type = TCMOD_TRANSFORM;
	}
	else
	{	
		MsgDev( D_WARN, "unknown 'tcMod' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TCMOD;

	return true;
}

/*
=================
R_ParseStageNextBundle
=================
*/
static bool R_ParseStageNextBundle( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	stageBundle_t	*bundle;
	token_t		tok;

	if( !GL_Support( R_ARB_MULTITEXTURE ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'nextBundle' without 'requires GL_ARB_multitexture'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_FRAGMENTPROGRAM )
	{
		if( stage->numBundles == gl_config.texturecoords )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_COORDS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}

		if( stage->numBundles == gl_config.teximageunits )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_IMAGE_UNITS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}
	}
	else
	{
		if( stage->numBundles == gl_config.textureunits )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_UNITS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}
	}

	if( stage->numBundles == MAX_TEXTURE_UNITS )
	{
		MsgDev( D_WARN, "MAX_TEXTURE_UNITS hit in shader '%s'\n", shader->name );
		return false;
	}
	bundle = stage->bundles[stage->numBundles++];

	if( !Com_ReadToken( script, false, &tok ))
	{
		if(!(stage->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			if((stage->flags & SHADERSTAGE_BLENDFUNC) && ((stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE) && GL_Support( R_TEXTURE_ENV_ADD_EXT )))
				bundle->texEnv = GL_ADD;
			else bundle->texEnv = GL_MODULATE;
		}
		else
		{
			if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE))
			{
				bundle->texEnv = GL_COMBINE_ARB;
				bundle->texEnvCombine.rgbCombine = GL_ADD;
				bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.rgbScale = 1;
				bundle->texEnvCombine.alphaCombine = GL_ADD;
				bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaScale = 1;
				bundle->texEnvCombine.constColor[0] = 1.0;
				bundle->texEnvCombine.constColor[1] = 1.0;
				bundle->texEnvCombine.constColor[2] = 1.0;
				bundle->texEnvCombine.constColor[3] = 1.0;
				bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
			}
			else
			{
				bundle->texEnv = GL_COMBINE_ARB;
				bundle->texEnvCombine.rgbCombine = GL_MODULATE;
				bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.rgbScale = 1;
				bundle->texEnvCombine.alphaCombine = GL_MODULATE;
				bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaScale = 1;
				bundle->texEnvCombine.constColor[0] = 1.0;
				bundle->texEnvCombine.constColor[1] = 1.0;
				bundle->texEnvCombine.constColor[2] = 1.0;
				bundle->texEnvCombine.constColor[3] = 1.0;
				bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
			}
		}
	}
	else
	{
		if( !com.stricmp( tok.string, "modulate" )) bundle->texEnv = GL_MODULATE;
		else if( !com.stricmp( tok.string, "add" ))
		{
			if( !GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			{
				MsgDev( D_WARN, "shader '%s' uses 'nextBundle add' without 'requires GL_ARB_texture_env_add'\n", shader->name );
				return false;
			}
			bundle->texEnv = GL_ADD;
		}
		else if( !com.stricmp( tok.string, "combine" ))
		{
			if( !GL_Support( R_COMBINE_EXT ))
			{
				MsgDev( D_WARN, "shader '%s' uses 'nextBundle combine' without 'requires GL_ARB_texture_env_combine'\n", shader->name );
				return false;
			}

			bundle->texEnv = GL_COMBINE_ARB;
			bundle->texEnvCombine.rgbCombine = GL_MODULATE;
			bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
			bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
			bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
			bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
			bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
			bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
			bundle->texEnvCombine.rgbScale = 1;
			bundle->texEnvCombine.alphaCombine = GL_MODULATE;
			bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
			bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
			bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
			bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaScale = 1;
			bundle->texEnvCombine.constColor[0] = 1.0;
			bundle->texEnvCombine.constColor[1] = 1.0;
			bundle->texEnvCombine.constColor[2] = 1.0;
			bundle->texEnvCombine.constColor[3] = 1.0;
			bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
		}
		else
		{
			MsgDev( D_WARN, "unknown 'nextBundle' parameter '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
	}
	stage->flags |= SHADERSTAGE_NEXTBUNDLE;
	return true;
}

/*
=================
R_ParseStageVertexProgram
=================
*/
static bool R_ParseStageVertexProgram( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( !GL_Support( R_VERTEX_PROGRAM_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'vertexProgram' without 'requires GL_ARB_vertex_program'\n", shader->name );
		return false;
	}

	if( shader->type == SHADER_NOMIP )
	{
		MsgDev( D_WARN, "'vertexProgram' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'vertexProgram' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'vertexProgram' in shader '%s'\n", shader->name );
		return false;
	}

	stage->vertexProgram = R_FindProgram( tok.string, GL_VERTEX_PROGRAM_ARB );
	if( !stage->vertexProgram )
	{
		MsgDev( D_WARN, "couldn't find vertex program '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_VERTEXPROGRAM;

	return true;
}

/*
=================
R_ParseStageFragmentProgram
=================
*/
static bool R_ParseStageFragmentProgram( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( !GL_Support( R_FRAGMENT_PROGRAM_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'fragmentProgram' without 'requires GL_ARB_fragment_program'\n", shader->name );
		return false;
	}

	if( shader->type == SHADER_NOMIP )
	{
		MsgDev( D_WARN, "'fragmentProgram' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'fragmentProgram' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'fragmentProgram' in shader '%s'\n", shader->name );
		return false;
	}

	stage->fragmentProgram = R_FindProgram( tok.string, GL_FRAGMENT_PROGRAM_ARB );
	if( !stage->fragmentProgram )
	{
		MsgDev( D_WARN, "couldn't find fragment program '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_FRAGMENTPROGRAM;

	return true;
}

/*
=================
R_ParseStageAlphaFunc
=================
*/
static bool R_ParseStageAlphaFunc( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'alphaFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'alphaFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "GT0" ))
	{
		stage->alphaFunc.func = GL_GREATER;
		stage->alphaFunc.ref = 0.0;
	}
	else if( !com.stricmp( tok.string, "LT128"))
	{
		stage->alphaFunc.func = GL_LESS;
		stage->alphaFunc.ref = 0.5;
	}
	else if( !com.stricmp( tok.string, "GE128"))
	{
		stage->alphaFunc.func = GL_GEQUAL;
		stage->alphaFunc.ref = 0.5;
	}
	else
	{
		if( !com.stricmp( tok.string, "GL_NEVER" ))
			stage->alphaFunc.func = GL_NEVER;
		else if( !com.stricmp( tok.string, "GL_ALWAYS" ))
			stage->alphaFunc.func = GL_ALWAYS;
		else if( !com.stricmp( tok.string, "GL_EQUAL" ))
			stage->alphaFunc.func = GL_EQUAL;
		else if( !com.stricmp( tok.string, "GL_NOTEQUAL" ))
			stage->alphaFunc.func = GL_NOTEQUAL;
		else if( !com.stricmp( tok.string, "GL_LEQUAL" ))
			stage->alphaFunc.func = GL_LEQUAL;
		else if( !com.stricmp( tok.string, "GL_GEQUAL" ))
			stage->alphaFunc.func = GL_GEQUAL;
		else if( !com.stricmp( tok.string, "GL_LESS" ))
			stage->alphaFunc.func = GL_LESS;
		else if( !com.stricmp( tok.string, "GL_GREATER" ))
			stage->alphaFunc.func = GL_GREATER;
		else
		{
			MsgDev( D_WARN, "unknown 'alphaFunc' parameter '%s' in shader '%s', defaulting to GL_GREATER\n", tok.string, shader->name );
			stage->alphaFunc.func = GL_GREATER;
		}

		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_WARN, "missing parameters for 'alphaFunc' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaFunc.ref = bound( 0.0, com.atof( tok.string ), 1.0 );
	}
	stage->flags |= SHADERSTAGE_ALPHAFUNC;

	return true;
}

/*
=================
R_ParseStageBlendFunc
=================
*/
static bool R_ParseStageBlendFunc( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'blendFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "add" ))
	{
		stage->blendFunc.src = GL_ONE;
		stage->blendFunc.dst = GL_ONE;
	}
	else if( !com.stricmp( tok.string, "filter" ))
	{
		stage->blendFunc.src = GL_DST_COLOR;
		stage->blendFunc.dst = GL_ZERO;
	}
	else if( !com.stricmp( tok.string, "blend" ))
	{
		stage->blendFunc.src = GL_SRC_ALPHA;
		stage->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		if( !com.stricmp( tok.string, "GL_ZERO"))
			stage->blendFunc.src = GL_ZERO;
		else if( !com.stricmp( tok.string, "GL_ONE"))
			stage->blendFunc.src = GL_ONE;
		else if( !com.stricmp( tok.string, "GL_DST_COLOR"))
			stage->blendFunc.src = GL_DST_COLOR;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_DST_COLOR"))
			stage->blendFunc.src = GL_ONE_MINUS_DST_COLOR;
		else if( !com.stricmp( tok.string, "GL_SRC_COLOR"))
			stage->blendFunc.src = GL_SRC_COLOR;
		else if( !com.stricmp( tok.string, "GL_SRC_ALPHA"))
			stage->blendFunc.src = GL_SRC_ALPHA;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_SRC_ALPHA"))
			stage->blendFunc.src = GL_ONE_MINUS_SRC_ALPHA;
		else if( !com.stricmp( tok.string, "GL_DST_ALPHA"))
			stage->blendFunc.src = GL_DST_ALPHA;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_DST_ALPHA"))
			stage->blendFunc.src = GL_ONE_MINUS_DST_ALPHA;
		else if( !com.stricmp( tok.string, "GL_SRC_ALPHA_SATURATE"))
			stage->blendFunc.src = GL_SRC_ALPHA_SATURATE;
		else
		{
			MsgDev( D_WARN, "unknown 'blendFunc' parameter '%s' in shader '%s', defaulting to GL_ONE\n", tok.string, shader->name );
			stage->blendFunc.src = GL_ONE;
		}

		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_WARN, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
			return false;
		}

		if( !com.stricmp( tok.string, "GL_ZERO" ))
			stage->blendFunc.dst = GL_ZERO;
		else if( !com.stricmp( tok.string, "GL_ONE" ))
			stage->blendFunc.dst = GL_ONE;
		else if( !com.stricmp( tok.string, "GL_SRC_COLOR" ))
			stage->blendFunc.dst = GL_SRC_COLOR;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_SRC_COLOR" ))
			stage->blendFunc.dst = GL_ONE_MINUS_SRC_COLOR;
		else if( !com.stricmp( tok.string, "GL_SRC_ALPHA" ))
			stage->blendFunc.dst = GL_SRC_ALPHA;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_SRC_ALPHA" ))
			stage->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		else if( !com.stricmp( tok.string, "GL_DST_ALPHA" ))
			stage->blendFunc.dst = GL_DST_ALPHA;
		else if( !com.stricmp( tok.string, "GL_ONE_MINUS_DST_ALPHA" ))
			stage->blendFunc.dst = GL_ONE_MINUS_DST_ALPHA;
		else
		{
			MsgDev( D_WARN, "unknown 'blendFunc' parameter '%s' in shader '%s', defaulting to GL_ONE\n", tok.string, shader->name );
			stage->blendFunc.dst = GL_ONE;
		}
	}
	stage->flags |= SHADERSTAGE_BLENDFUNC;

	return true;
}

/*
=================
R_ParseStageDepthFunc
=================
*/
static bool R_ParseStageDepthFunc( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'depthFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'depthFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "lequal" ))
		stage->depthFunc.func = GL_LEQUAL;
	else if( !com.stricmp( tok.string, "equal" ))
		stage->depthFunc.func = GL_EQUAL;
	else
	{
		MsgDev( D_WARN, "unknown 'depthFunc' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DEPTHFUNC;

	return true;
}

/*
=================
R_ParseStageDepthWrite
=================
*/
static bool R_ParseStageDepthWrite( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'depthWrite' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DEPTHWRITE;
	return true;
}

/*
=================
R_ParseStageDetail
=================
*/
static bool R_ParseStageDetail( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'detail' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DETAIL;

	return true;
}

/*
=================
R_ParseStageRgbGen
=================
*/
static bool R_ParseStageRgbGen( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;
	int	i;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'rgbGen' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'rgbGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "identity" ))
		stage->rgbGen.type = RGBGEN_IDENTITY;
	else if( !com.stricmp( tok.string, "identityLighting" ))
		stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
	else if( !com.stricmp( tok.string, "wave" ))
	{
		if( !R_ParseWaveFunc( shader, &stage->rgbGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'rgbGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->rgbGen.type = RGBGEN_WAVE;
	}
	else if( !com.stricmp( tok.string, "colorWave"))
	{
		for( i = 0; i < 3; i++ )
		{
			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
				return false;
			}
			stage->rgbGen.params[i] = bound( 0.0, com.atof( tok.string ), 1.0 );
		}

		if( !R_ParseWaveFunc(shader, &stage->rgbGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->rgbGen.type = RGBGEN_COLORWAVE;
	}
	else if( !com.stricmp( tok.string, "vertex" ))
		stage->rgbGen.type = RGBGEN_VERTEX;
	else if( !com.stricmp( tok.string, "oneMinusVertex" ))
		stage->rgbGen.type = RGBGEN_ONEMINUSVERTEX;
	else if( !com.stricmp( tok.string, "entity" ))
		stage->rgbGen.type = RGBGEN_ENTITY;
	else if( !com.stricmp( tok.string, "oneMinusEntity" ))
		stage->rgbGen.type = RGBGEN_ONEMINUSENTITY;
	else if( !com.stricmp( tok.string, "lightingAmbient" ))
		stage->rgbGen.type = RGBGEN_LIGHTINGAMBIENT;
	else if( !com.stricmp( tok.string, "lightingDiffuse" ))
		stage->rgbGen.type = RGBGEN_LIGHTINGDIFFUSE;
	else if( !com.stricmp( tok.string, "const" ) || !com.stricmp( tok.string, "constant" ))
	{
		for( i = 0; i < 3; i++ )
		{
			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'rgbGen const' in shader '%s'\n", shader->name );
				return false;
			}
			stage->rgbGen.params[i] = bound( 0.0, com.atof( tok.string ), 1.0 );
		}
		stage->rgbGen.type = RGBGEN_CONST;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'rgbGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_RGBGEN;

	return true;
}

/*
=================
R_ParseStageAlphaGen
=================
*/
static bool R_ParseStageAlphaGen( ref_shader_t *shader, shaderStage_t *stage, script_t *script )
{
	token_t	tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'alphaGen' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_WARN, "missing parameters for 'alphaGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "identity" ))
		stage->alphaGen.type = ALPHAGEN_IDENTITY;
	else if( !com.stricmp( tok.string, "wave" ))
	{
		if( !R_ParseWaveFunc( shader, &stage->alphaGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'alphaGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.type = ALPHAGEN_WAVE;
	}
	else if( !com.stricmp( tok.string, "alphaWave" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_WARN, "missing parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}

		stage->alphaGen.params[0] = bound( 0.0, com.atof( tok.string ), 1.0 );

		if(!R_ParseWaveFunc( shader, &stage->alphaGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.type = ALPHAGEN_ALPHAWAVE;
	}
	else if( !com.stricmp( tok.string, "vertex" ))
		stage->alphaGen.type = ALPHAGEN_VERTEX;
	else if( !com.stricmp( tok.string, "oneMinusVertex" ))
		stage->alphaGen.type = ALPHAGEN_ONEMINUSVERTEX;
	else if( !com.stricmp( tok.string, "entity" ))
		stage->alphaGen.type = ALPHAGEN_ENTITY;
	else if( !com.stricmp( tok.string, "oneMinusEntity" ))
		stage->alphaGen.type = ALPHAGEN_ONEMINUSENTITY;
	else if( !com.stricmp( tok.string, "dot" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 1.0;
		}
		else
		{
			stage->alphaGen.params[0] = bound( 0.0, com.atof( tok.string ), 1.0 );

			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen dot' in shader '%s'\n", shader->name );
				return false;
			}
			stage->alphaGen.params[1] = bound( 0.0, com.atof( tok.string ), 1.0 );
		}
		stage->alphaGen.type = ALPHAGEN_DOT;
	}
	else if( !com.stricmp( tok.string, "oneMinusDot" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 1.0;
		}
		else
		{
			stage->alphaGen.params[0] = bound( 0.0, com.atof( tok.string ), 1.0 );

			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen oneMinusDot' in shader '%s'\n", shader->name );
				return false;
			}
			stage->alphaGen.params[1] = bound( 0.0, com.atof( tok.string ), 1.0 );
		}
		stage->alphaGen.type = ALPHAGEN_ONEMINUSDOT;
	}
	else if( !com.stricmp( tok.string, "fade" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 256.0;
			stage->alphaGen.params[2] = 1.0 / 256.0;
		}
		else
		{
			stage->alphaGen.params[0] = com.atof( tok.string );

			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen fade' in shader '%s'\n", shader->name );
				return false;
			}

			stage->alphaGen.params[1] = com.atof( tok.string );
			stage->alphaGen.params[2] = stage->alphaGen.params[1] - stage->alphaGen.params[0];
			if( stage->alphaGen.params[2] ) stage->alphaGen.params[2] = 1.0 / stage->alphaGen.params[2];
		}
		stage->alphaGen.type = ALPHAGEN_FADE;
	}
	else if( !com.stricmp( tok.string, "oneMinusFade" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 256.0;
			stage->alphaGen.params[2] = 1.0 / 256.0;
		}
		else
		{
			stage->alphaGen.params[0] = com.atof( tok.string );

			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen oneMinusFade' in shader '%s'\n", shader->name );
				return false;
			}

			stage->alphaGen.params[1] = com.atof( tok.string );
			stage->alphaGen.params[2] = stage->alphaGen.params[1] - stage->alphaGen.params[0];
			if( stage->alphaGen.params[2] ) stage->alphaGen.params[2] = 1.0 / stage->alphaGen.params[2];
		}
		stage->alphaGen.type = ALPHAGEN_ONEMINUSFADE;
	}
	else if( !com.stricmp( tok.string, "lightingSpecular" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
			stage->alphaGen.params[0] = 5.0;
		else
		{
			stage->alphaGen.params[0] = com.atof( tok.string );
			if( stage->alphaGen.params[0] <= 0.0 )
			{
				MsgDev( D_WARN, "invalid exponent value of %f for 'alphaGen lightingSpecular' in shader '%s', defaulting to 5\n", stage->alphaGen.params[0], shader->name );
				stage->alphaGen.params[0] = 5.0;
			}
		}
		stage->alphaGen.type = ALPHAGEN_LIGHTINGSPECULAR;
	}
	else if( !com.stricmp( tok.string, "const" ) || !com.stricmp( tok.string, "constant" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_WARN, "missing parameters for 'alphaGen const' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.params[0] = bound( 0.0, com.atof( tok.string ), 1.0 );
		stage->alphaGen.type = ALPHAGEN_CONST;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'alphaGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_ALPHAGEN;

	return true;
}


// =====================================================================

typedef struct
{
	const char	*name;
	bool		(*parseFunc)( ref_shader_t *shader, script_t *script );
} shaderGeneralCmd_t;

typedef struct
{
	const char	*name;
	bool		(*parseFunc)( ref_shader_t *shader, shaderStage_t *stage, script_t *script );
} shaderStageCmd_t;

static shaderGeneralCmd_t r_shaderGeneralCmds[] =
{
{"surfaceParm",	R_ParseGeneralSurfaceParm	},
{ "noPicMip",	R_ParseGeneralNoPicmip	},
{ "noCompress",	R_ParseGeneralNoCompress	},
{ "highQuality",	R_ParseGeneralHighQuality	},
{ "linear",	R_ParseGeneralLinear	},
{ "nearest",	R_ParseGeneralNearest	},
{ "clamp",	R_ParseGeneralClamp		},
{ "zeroClamp",	R_ParseGeneralZeroClamp	},
{ "alphaZeroClamp",	R_ParseGeneralAlphaZeroClamp	},
{"noShadows",	R_ParseGeneralNoShadows	},
{"nomarks",	R_ParseGeneralNoFragments	},
{"entityMergable",	R_ParseGeneralEntityMergable	},
{"polygonOffset",	R_ParseGeneralPolygonOffset	},
{"cull",		R_ParseGeneralCull		},
{"sort",		R_ParseGeneralSort		},
{"mirror",	R_ParseGeneralMirror	},
{"tessSize",	R_ParseGeneralTessSize	},
{"skyParms",	R_ParseGeneralSkyParms	},
{"deformVertexes",	R_ParseGeneralDeformVertexes	},
{NULL, NULL }	// terminator
};

static shaderStageCmd_t r_shaderStageCmds[] =
{
{"if",		R_ParseStageIf		},
{"noPicMip",	R_ParseStageNoPicMip	},
{"noCompress",	R_ParseStageNoCompress	},
{"highQuality",	R_ParseStageHighQuality	},
{"nearest",	R_ParseStageNearest		},
{"linear",	R_ParseStageLinear		},
{"noClamp",	R_ParseStageNoClamp		},
{"clamp",		R_ParseStageClamp		},
{"zeroClamp",	R_ParseStageZeroClamp	},
{"alphaZeroClamp",	R_ParseStageAlphaZeroClamp	},
{"animFrequency",	R_ParseStageAnimFrequency	},
{"map",		R_ParseStageMap		},
{"bumpMap",	R_ParseStageBumpMap		},
{"cubeMap",	R_ParseStageCubeMap		},
{"videoMap",	R_ParseStageVideoMap	},
{"texEnvCombine",	R_ParseStageTexEnvCombine	},
{"tcGen",		R_ParseStageTcGen		},
{"tcMod",		R_ParseStageTcMod		},
{"nextBundle",	R_ParseStageNextBundle	},
{"vertexProgram",	R_ParseStageVertexProgram	},
{"fragmentProgram",	R_ParseStageFragmentProgram	},
{"alphaFunc",	R_ParseStageAlphaFunc	},
{"blendFunc",	R_ParseStageBlendFunc	},
{"depthFunc",	R_ParseStageDepthFunc	},
{"depthWrite",	R_ParseStageDepthWrite	},
{"detail",	R_ParseStageDetail		},
{"rgbGen",	R_ParseStageRgbGen		},
{"alphaGen",	R_ParseStageAlphaGen	},
{NULL, NULL }	// terminator
};

/*
=================
R_ParseShaderCommand
=================
*/
static bool R_ParseShaderCommand( ref_shader_t *shader, script_t *script, char *command )
{
	shaderGeneralCmd_t	*cmd;

	for( cmd = r_shaderGeneralCmds; cmd->name != NULL; cmd++ )
	{
		if( !com.stricmp( cmd->name, command ))
			return cmd->parseFunc( shader, script );
	}

	// compiler commands ignored silently
	if( !com.strnicmp( "q3map_", command, 6 ) || !com.strnicmp( "rad_", command, 4 ))
	{
		Com_SkipRestOfLine( script );
		return true;
	}
	MsgDev( D_WARN, "unknown general command '%s' in shader '%s'\n", command, shader->name );
	return false;
}

/*
=================
R_ParseShaderStageCommand
=================
*/
static bool R_ParseShaderStageCommand( ref_shader_t *shader, shaderStage_t *stage, script_t *script, char *command )
{
	shaderStageCmd_t	*cmd;

	for( cmd = r_shaderStageCmds; cmd->name != NULL; cmd++ )
	{
		if(!com.stricmp( cmd->name, command ))
			return cmd->parseFunc( shader, stage, script );
	}
	MsgDev( D_WARN, "unknown stage command '%s' in shader '%s'\n", command, shader->name );
	return false;
}

/*
=================
R_ParseShader
=================
*/
static bool R_ParseShader( ref_shader_t *shader, script_t *script )
{
	shaderStage_t	*stage;
	token_t		tok;

	if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
	{
		MsgDev( D_WARN, "shader '%s' has an empty script\n", shader->name );
		return false;
	}

	// parse the shader
	if( !com.stricmp( tok.string, "{" ))
	{
		while( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
			{
				MsgDev( D_WARN, "no concluding '}' in shader '%s'\n", shader->name );
				return false;	// End of data
			}

			if( !com.stricmp( tok.string, "}" )) break; // end of shader

			// parse a stage
			if( !com.stricmp( tok.string, "{" ))
			{
				// create a new stage
				if( shader->numStages == SHADER_MAX_STAGES )
				{
					MsgDev( D_WARN, "SHADER_MAX_STAGES hit in shader '%s'\n", shader->name );
					return false;
				}
				stage = shader->stages[shader->numStages++];
				stage->numBundles++;

				// parse it
				while( 1 )
				{
					if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
					{
						MsgDev( D_WARN, "no matching '}' in shader '%s'\n", shader->name );
						return false; // end of data
					}

					if( !com.stricmp( tok.string, "}" )) break; // end of stage

					// parse the command
					if( !R_ParseShaderStageCommand( shader, stage, script, tok.string ))
						return false;
				}
				continue;
			}

			// parse the command
			if( !R_ParseShaderCommand( shader, script, tok.string ))
				return false;
		}
		return true;
	}
	else
	{
		MsgDev( D_WARN, "expected '{', found '%s' instead in shader '%s'\n", shader->name );
		return false;
	}
}

/*
=================
R_ParseShaderFile
=================
*/
static void R_ParseShaderFile( script_t *script, const char *name )
{
	ref_script_t	*shaderScript;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	script_t		*scriptBlock;
	bool		clamp = false;
	bool		snap = false;
	char		*buffer, *end;
	uint		i, hashKey;
	int		tableStatus = 0;
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
			if( !com.stricmp( tok.string, "clamp" )) clamp = true;
			else if( !com.stricmp( tok.string, "snap" )) snap = true;
			else if( !com.stricmp( tok.string, "table" ))
			{
				if( !R_ParseTable( script, clamp, snap ))
					tableStatus = -1; // parsing failed
				else tableStatus = 1;
				break;
			} 
			else
			{
				tableStatus = 0;  // not a table
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
		com.strncpy( shaderScript->name, tok.string, sizeof( shaderScript->name ));
		shaderScript->surfaceParm = 0;
		shaderScript->type = -1;

		com.strncpy( shaderScript->source, name, sizeof( shaderScript->source ));
		shaderScript->line = tok.line;
		shaderScript->buffer = Mem_Alloc( r_shaderpool, size + 1 );
		Mem_Copy( shaderScript->buffer, buffer, size );
		shaderScript->buffer[size] = 0; // terminator
		shaderScript->size = size;

		// add to hash table
		hashKey = Com_HashKey( shaderScript->name, SHADERS_HASH_SIZE );
		shaderScript->nextHash = r_shaderScriptsHash[hashKey];
		r_shaderScriptsHash[hashKey] = shaderScript;

		// we must parse surfaceParm commands here, because R_FindShader
		// needs this for correct shader loading.
		// proper syntax checking is done when the shader is loaded.
		scriptBlock = Com_OpenScript( shaderScript->name, shaderScript->buffer, shaderScript->size );
		if( scriptBlock )
		{
			while( 1 )
			{
				if( !Com_ReadToken( scriptBlock, SC_ALLOW_NEWLINES, &tok ))
					break; // end of data

				if( !com.stricmp( tok.string, "surfaceParm" ))
				{
					if( !Com_ReadToken( scriptBlock, 0, &tok ))
						continue;

					for( i = 0; i < numInfoParms; i++ )
					{
						if( !com.stricmp(tok.string, infoParms[i].name ))
						{
							shaderScript->surfaceParm |= infoParms[i].surfaceFlags;
							break;
						}
					}
					if( shaderScript->surfaceParm & SURF_SKY )
						shaderScript->type = SHADER_SKY; 
					else shaderScript->type = SHADER_TEXTURE;
				}
			}
			Com_CloseScript( scriptBlock );
		}
	}
}

/*
=======================================================================

 SHADER INITIALIZATION AND LOADING

=======================================================================
*/
/*
=================
R_NewShader
=================
*/
static ref_shader_t *R_NewShader( void )
{
	ref_shader_t	*shader;
	int		i, j;

	// find a free shader_t slot
	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		if( !shader->name[0] ) break;

	if( i == r_numShaders )
	{
		if( r_numShaders == MAX_SHADERS )
		{
			Host_Error( "R_LoadShader: MAX_SHADERS limit exceeded\n" );
			return tr.defaultShader;
		}
		r_numShaders++;
	}

	shader = &r_parseShader;
	Mem_Set( shader, 0, sizeof( ref_shader_t ));
	shader->shadernum = i;

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
	{
		shader->stages[i] = &r_parseShaderStages[i];
		Mem_Set( shader->stages[i], 0, sizeof( shaderStage_t ));

		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
		{
			shader->stages[i]->bundles[j] = &r_parseStageTMU[i][j];
			Mem_Set( shader->stages[i]->bundles[j], 0, sizeof( stageBundle_t ));
		}
	}

	shader->statements = r_parseShaderOps;
	Mem_Set( shader->statements, 0, MAX_EXPRESSION_OPS * sizeof( statement_t ));

	shader->expressions = r_parseShaderExpressionRegisters;
	Mem_Set( shader->expressions, 0, MAX_EXPRESSION_REGISTERS * sizeof( float ));

	return shader;
}

/*
=================
R_CreateDefaultShader
=================
*/
static ref_shader_t *R_CreateDefaultShader( const char *name, int shaderType, uint surfaceParm )
{
	ref_shader_t	*shader;
	const byte	*buffer = NULL; // default image buffer
	size_t		bufsize = 0;
	int		i;

	// clear static shader
	shader = R_NewShader();

	// fill it in
	com.strncpy( shader->name, name, sizeof( shader->name ));
	shader->type = shaderType;
	shader->surfaceParm = surfaceParm;

	switch( shader->type )
	{
	case SHADER_SKY:
		shader->flags |= SHADER_SKYPARMS;

		for( i = 0; i < 6; i++ )
		{
			shader->skyParms.farBox[i] = R_FindTexture(va("gfx/env/%s%s", shader->name, r_skyBoxSuffix[i]), NULL, 0, 0, TF_LINEAR, TW_CLAMP );
			if( !shader->skyParms.farBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
				shader->skyParms.farBox[i] = r_defaultTexture;
			}
		}
		shader->skyParms.cloudHeight = 128.0;
		break;
	case SHADER_TEXTURE:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, 0, 0, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->bundles[0]->numTextures++;

		// fast presets
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
		}
		else if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
		}
		if( shader->surfaceParm & SURF_WARP )
		{
			shader->flags |= SHADER_TESSSIZE;
			shader->tessSize = 64;

			shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_TCGEN;
			shader->stages[0]->bundles[0]->tcGen.type = TCGEN_WARP;
		}
		else if( shader->surfaceParm & SURF_SKY )
		{
			shader->surfaceParm |= SURF_NOLIGHTMAP;
		}
		shader->stages[0]->numBundles++;
		shader->numStages++;

		if(!( shader->surfaceParm & (SURF_NOLIGHTMAP|SURF_LIGHT)))
		{
			shader->flags |= SHADER_HASLIGHTMAP;
			shader->stages[1]->bundles[0]->flags |= STAGEBUNDLE_MAP;
			shader->stages[1]->bundles[0]->texType = TEX_LIGHTMAP;
			shader->stages[1]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[1]->blendFunc.src = GL_DST_COLOR;
			shader->stages[1]->blendFunc.dst = GL_ZERO;
			shader->stages[1]->numBundles++;
			shader->numStages++;
		}
		break;
	case SHADER_STUDIO:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = r_internalMiptex; // internal spriteframe
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		// fast presets
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_ADDITIVE;
		}
		if( shader->surfaceParm & SURF_ADDITIVE )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_ADDITIVE;
		}
		if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
			shader->sort = SORT_SEETHROUGH;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_SPRITE:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = r_internalMiptex; // internal spriteframe
		shader->stages[0]->bundles[0]->texType = TEX_GENERIC;
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find spriteframe for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_ADDITIVE;
		}
		if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
			shader->sort = SORT_SEETHROUGH;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_FONT:
		// don't let user set invalid font
		buffer = FS_LoadInternal( "default.dds", &bufsize );
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, TF_NOPICMIP, TF_LINEAR, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->rgbGen.type = RGBGEN_VERTEX;
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC|SHADERSTAGE_RGBGEN;
		shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
		shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_NOMIP:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, TF_NOPICMIP, TF_LINEAR, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->rgbGen.type = RGBGEN_VERTEX;
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC|SHADERSTAGE_RGBGEN;
		shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
		shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_GENERIC:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, 0, 0, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	}
	return shader;
}

/*
=================
R_CreateShader
=================
*/
static ref_shader_t *R_CreateShader( const char *name, int shaderType, uint surfaceParm, ref_script_t *shaderScript )
{
	ref_shader_t	*shader;
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	script_t		*script;
	int		i, j;

	// clear static shader
	shader = R_NewShader();

	// fill it in
	com.strncpy( shader->name, name, sizeof( shader->name ));
	shader->type = shaderType;
	shader->surfaceParm = surfaceParm;
	shader->flags = SHADER_EXTERNAL;

	if( shaderType == SHADER_NOMIP ) shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);

	// if we have a script, create an external shader
	if( shaderScript )
	{
		shader->shaderScript = shaderScript;

		// load the script text
		script = Com_OpenScript( shaderScript->name, shaderScript->buffer, shaderScript->size );
		if( !script ) return R_CreateDefaultShader( name, shaderType, surfaceParm );

		// parse it
		if( !R_ParseShader( shader, script ))
		{
			// invalid script, so make sure we stop cinematics
			for( i = 0; i < shader->numStages; i++ )
			{
				stage = shader->stages[i];

				for( j = 0; j < stage->numBundles; j++ )
				{
					bundle = stage->bundles[j];

					// FIXME: implement
					// if( bundle->flags & STAGEBUNDLE_VIDEOMAP )
					//	CIN_StopCinematic( bundle->cinematicHandle );
				}
			}

			// use a default shader instead
			shader = R_NewShader();

			com.strncpy( shader->name, name, sizeof( shader->name ));
			shader->type = shaderType;
			shader->surfaceParm = surfaceParm;
			shader->flags = SHADER_EXTERNAL|SHADER_DEFAULTED;
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
			shader->stages[0]->bundles[0]->numTextures++;
			shader->stages[0]->numBundles++;
			shader->numStages++;
		}

		Com_CloseScript( script );
		return shader;
	}

	return R_CreateDefaultShader( name, shaderType, surfaceParm );
}

/*
=================
R_FinishShader
=================
*/
static void R_FinishShader( ref_shader_t *shader )
{
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j;

	// remove entityMergable from 2D shaders
	if( shader->type == SHADER_NOMIP )
		shader->flags &= ~SHADER_ENTITYMERGABLE;

	// remove polygonOffset from 2D shaders
	if( shader->type == SHADER_NOMIP )
		shader->flags &= ~SHADER_POLYGONOFFSET;

	// set cull if unset
	if(!(shader->flags & SHADER_CULL ))
	{
		shader->flags |= SHADER_CULL;
		shader->cull.mode = GL_FRONT;
	}
	else
	{
		// remove if undefined (disabled)
		if( shader->cull.mode == 0 )
			shader->flags &= ~SHADER_CULL;
	}

	// remove cull from 2D shaders
	if( shader->type == SHADER_NOMIP )
	{
		shader->flags &= ~SHADER_CULL;
		shader->cull.mode = 0;
	}

	// make sure sky shaders have a cloudHeight value
	if( shader->type == SHADER_SKY )
	{
		if(!(shader->flags & SHADER_SKYPARMS))
		{
			shader->flags |= SHADER_SKYPARMS;
			shader->skyParms.cloudHeight = 128.0;
		}
	}

	// remove deformVertexes from 2D shaders
	if( shader->type == SHADER_NOMIP )
	{
		if( shader->flags & SHADER_DEFORMVERTEXES )
		{
			shader->flags &= ~SHADER_DEFORMVERTEXES;
			for( i = 0; i < shader->numDeforms; i++ )
				Mem_Set( &shader->deform[i], 0, sizeof( deform_t ));
			shader->numDeforms = 0;
		}
	}

	// lightmap but no lightmap stage?
	if( shader->type == SHADER_TEXTURE && !(shader->surfaceParm & SURF_NOLIGHTMAP ))
	{
		if(!(shader->flags & SHADER_DEFAULTED) && !(shader->flags & SHADER_HASLIGHTMAP))
			MsgDev( D_WARN, "shader '%s' has lightmap but no lightmap stage!\n", shader->name );
	}

	// check stages
	for( i = 0; i < shader->numStages; i++ )
	{
		stage = shader->stages[i];

		// check bundles
		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			// make sure it has a texture
			if( bundle->texType == TEX_GENERIC && !bundle->numTextures )
			{
				if( j == 0 ) MsgDev( D_WARN, "shader '%s' has a stage with no texture!\n", shader->name );
				else MsgDev( D_WARN, "shader '%s' has a bundle with no texture!\n", shader->name );
				bundle->textures[bundle->numTextures++] = r_defaultTexture;
			}

			// set tcGen if unset
			if(!(bundle->flags & STAGEBUNDLE_TCGEN))
			{
				if( bundle->texType == TEX_LIGHTMAP )
					bundle->tcGen.type = TCGEN_LIGHTMAP;
				else bundle->tcGen.type = TCGEN_BASE;
				bundle->flags |= STAGEBUNDLE_TCGEN;
			}
			else
			{
				// only allow tcGen lightmap on world surfaces
				if( bundle->tcGen.type == TCGEN_LIGHTMAP )
				{
					if( shader->type != SHADER_TEXTURE )
						bundle->tcGen.type = TCGEN_BASE;
				}
			}

			// check keywords that reference the current entity
			if( bundle->flags & STAGEBUNDLE_TCGEN )
			{
				switch( bundle->tcGen.type )
				{
				case TCGEN_ENVIRONMENT:
				case TCGEN_LIGHTVECTOR:
				case TCGEN_HALFANGLE:
					if( shader->type == SHADER_NOMIP )
						bundle->tcGen.type = TCGEN_BASE;
					shader->flags &= ~SHADER_ENTITYMERGABLE;
					break;
				}
			}
		}

		// blendFunc GL_ONE GL_ZERO is non-blended
		if( stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ZERO )
		{
			stage->flags &= ~SHADERSTAGE_BLENDFUNC;
			stage->blendFunc.src = 0;
			stage->blendFunc.dst = 0;
		}

		// set depthFunc if unset
		if(!(stage->flags & SHADERSTAGE_DEPTHFUNC))
		{
			stage->flags |= SHADERSTAGE_DEPTHFUNC;
			stage->depthFunc.func = GL_LEQUAL;
		}

		// Remove depthFunc from 2D shaders
		if( shader->type == SHADER_NOMIP )
		{
			stage->flags &= ~SHADERSTAGE_DEPTHFUNC;
			stage->depthFunc.func = 0;
		}

		// set depthWrite for non-blended stages
		if(!(stage->flags & SHADERSTAGE_BLENDFUNC))
			stage->flags |= SHADERSTAGE_DEPTHWRITE;

		// remove depthWrite from sky and 2D shaders
		if( shader->type == SHADER_SKY || shader->type == SHADER_NOMIP )
			stage->flags &= ~SHADERSTAGE_DEPTHWRITE;

		// ignore detail stages if detail textures are disabled
		if( stage->flags & SHADERSTAGE_DETAIL )
		{
			if( !r_detailtextures->integer )
				stage->ignore = true;
		}

		// set rgbGen if unset
		if(!(stage->flags & SHADERSTAGE_RGBGEN))
		{
			switch( shader->type )
			{
			case SHADER_SKY:
				stage->rgbGen.type = RGBGEN_IDENTITY;
				break;
			case SHADER_TEXTURE:
				if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->bundles[0]->texType != TEX_LIGHTMAP))
					stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
				else stage->rgbGen.type = RGBGEN_IDENTITY;
				break;
			case SHADER_STUDIO:
				if( shader->surfaceParm & SURF_ADDITIVE )
					stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
				else stage->rgbGen.type = RGBGEN_LIGHTINGAMBIENT;
				break;
			case SHADER_SPRITE:
				if( shader->surfaceParm & SURF_ALPHA|SURF_BLEND )
					stage->rgbGen.type = RGBGEN_LIGHTINGAMBIENT; // sprite colormod
				else if( shader->surfaceParm & SURF_ADDITIVE )
					stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
				break;
			case SHADER_NOMIP:
			case SHADER_GENERIC:
				stage->rgbGen.type = RGBGEN_VERTEX;
				break;
			}
			stage->flags |= SHADERSTAGE_RGBGEN;
		}

		// set alphaGen if unset
		if(!(stage->flags & SHADERSTAGE_ALPHAGEN))
		{
			switch( shader->type )
			{
			case SHADER_SKY:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_TEXTURE:
				if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->bundles[0]->texType != TEX_LIGHTMAP))
				{
					if( shader->surfaceParm & SURF_ADDITIVE )
						stage->alphaGen.type = ALPHAGEN_VERTEX;
					else stage->alphaGen.type = ALPHAGEN_IDENTITY;
				}
				else stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_STUDIO:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_SPRITE:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_NOMIP:
			case SHADER_GENERIC:
				stage->alphaGen.type = ALPHAGEN_VERTEX;
				break;
			}
			stage->flags |= SHADERSTAGE_ALPHAGEN;
		}

		// check keywords that reference the current entity
		if( stage->flags & SHADERSTAGE_RGBGEN )
		{
			switch( stage->rgbGen.type )
			{
			case RGBGEN_ENTITY:
			case RGBGEN_ONEMINUSENTITY:
			case RGBGEN_LIGHTINGAMBIENT:
			case RGBGEN_LIGHTINGDIFFUSE:
				if( shader->type == SHADER_NOMIP )
					stage->rgbGen.type = RGBGEN_VERTEX;
				shader->flags &= ~SHADER_ENTITYMERGABLE;
				break;
			}
		}
		if( stage->flags & SHADERSTAGE_ALPHAGEN )
		{
			switch( stage->alphaGen.type )
			{
			case ALPHAGEN_ENTITY:
			case ALPHAGEN_ONEMINUSENTITY:
			case ALPHAGEN_DOT:
			case ALPHAGEN_ONEMINUSDOT:
			case ALPHAGEN_FADE:
			case ALPHAGEN_ONEMINUSFADE:
			case ALPHAGEN_LIGHTINGSPECULAR:
				if( shader->type == SHADER_NOMIP )
					stage->alphaGen.type = ALPHAGEN_VERTEX;
				shader->flags &= ~SHADER_ENTITYMERGABLE;
				break;
			}
		}

		// set texEnv for first bundle
		if(!(stage->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			if(!(stage->flags & SHADERSTAGE_BLENDFUNC))
			{
				if( stage->rgbGen.type == RGBGEN_IDENTITY && stage->alphaGen.type == ALPHAGEN_IDENTITY)
					stage->bundles[0]->texEnv = GL_REPLACE;
				else stage->bundles[0]->texEnv = GL_MODULATE;
			}
			else
			{
				if((stage->blendFunc.src == GL_DST_COLOR && stage->blendFunc.dst == GL_ZERO)
				|| (stage->blendFunc.src == GL_ZERO && stage->blendFunc.dst == GL_SRC_COLOR))
					stage->bundles[0]->texEnv = GL_MODULATE;
				else if(stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE)
					stage->bundles[0]->texEnv = GL_ADD;
			}
		}

		// if this stage is ignored, make sure we stop cinematics
		if( stage->ignore )
		{
			for( j = 0; j < stage->numBundles; j++ )
			{
				bundle = stage->bundles[j];

				//FIXME: implement
				//if( bundle->flags & STAGEBUNDLE_VIDEOMAP )
				//	CIN_StopCinematic( bundle->cinematicHandle );
			}
		}
	}

	// set sort if unset
	if( !(shader->flags & SHADER_SORT ))
	{
		if( shader->type == SHADER_SKY )
			shader->sort = SORT_SKY;
		else
		{
			stage = shader->stages[0];
			if( !(stage->flags & SHADERSTAGE_BLENDFUNC ))
				shader->sort = SORT_OPAQUE;
			else
			{
				if( shader->flags & SHADER_POLYGONOFFSET )
					shader->sort = SORT_DECAL;
				else if( stage->flags & SHADERSTAGE_ALPHAFUNC )
					shader->sort = SORT_SEETHROUGH;
				else
				{
					if((stage->blendFunc.src == GL_SRC_ALPHA && stage->blendFunc.dst == GL_ONE) || (stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE))
						shader->sort = SORT_ADDITIVE;
					else shader->sort = SORT_ADDITIVE;
				}
			}
		}
	}
}

/*
=================
R_MergeShaderStages
=================
*/
static bool R_MergeShaderStages( shaderStage_t *stage1, shaderStage_t *stage2 )
{
	if( GL_Support( R_ARB_MULTITEXTURE ))
		return false;

	if( stage1->numBundles == gl_config.textureunits || stage1->numBundles == MAX_TEXTURE_UNITS )
		return false;

	if( stage1->flags & SHADERSTAGE_NEXTBUNDLE || stage2->flags & SHADERSTAGE_NEXTBUNDLE )
		return false;

	if( stage1->flags & (SHADERSTAGE_VERTEXPROGRAM | SHADERSTAGE_FRAGMENTPROGRAM) || stage2->flags & (SHADERSTAGE_VERTEXPROGRAM | SHADERSTAGE_FRAGMENTPROGRAM))
		return false;

	if( stage2->flags & SHADERSTAGE_ALPHAFUNC )
		return false;

	if( stage1->flags & SHADERSTAGE_ALPHAFUNC || stage1->depthFunc.func == GL_EQUAL )
	{
		if( stage2->depthFunc.func != GL_EQUAL )
			return false;
	}

	if(!(stage1->flags & SHADERSTAGE_DEPTHWRITE))
	{
		if( stage2->flags & SHADERSTAGE_DEPTHWRITE )
			return false;
	}

	if( stage2->rgbGen.type != RGBGEN_IDENTITY || stage2->alphaGen.type != ALPHAGEN_IDENTITY )
		return false;

	switch( stage1->bundles[0]->texEnv )
	{
	case GL_REPLACE:
		if( stage2->bundles[0]->texEnv == GL_REPLACE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_MODULATE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_ADD && GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			return true;
		break;
	case GL_MODULATE:
		if( stage2->bundles[0]->texEnv == GL_REPLACE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_MODULATE )
			return true;
		break;
	case GL_ADD:
		if( stage2->bundles[0]->texEnv == GL_ADD && GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			return true;
		break;
	}
	return false;
}

/*
=================
R_OptimizeShader
=================
*/
static void R_OptimizeShader( ref_shader_t *shader )
{
	shaderStage_t	*curStage, *prevStage = NULL;
	float		*registers = shader->expressions;
	statement_t	*op;
	int		i;

	// Make sure the predefined registers are initialized
	registers[EXP_REGISTER_ONE] = 1.0;
	registers[EXP_REGISTER_ZERO] = 0.0;
	registers[EXP_REGISTER_TIME] = 0.0;
	registers[EXP_REGISTER_PARM0] = 0.0;
	registers[EXP_REGISTER_PARM1] = 0.0;
	registers[EXP_REGISTER_PARM2] = 0.0;
	registers[EXP_REGISTER_PARM3] = 0.0;
	registers[EXP_REGISTER_PARM4] = 0.0;
	registers[EXP_REGISTER_PARM5] = 0.0;
	registers[EXP_REGISTER_PARM6] = 0.0;
	registers[EXP_REGISTER_PARM7] = 0.0;
	registers[EXP_REGISTER_GLOBAL0] = 0.0;
	registers[EXP_REGISTER_GLOBAL1] = 0.0;
	registers[EXP_REGISTER_GLOBAL2] = 0.0;
	registers[EXP_REGISTER_GLOBAL3] = 0.0;
	registers[EXP_REGISTER_GLOBAL4] = 0.0;
	registers[EXP_REGISTER_GLOBAL5] = 0.0;
	registers[EXP_REGISTER_GLOBAL6] = 0.0;
	registers[EXP_REGISTER_GLOBAL7] = 0.0;

	// try to merge multiple stages for multitexturing
	for( i = 0; i < shader->numStages; i++ )
	{
		curStage = shader->stages[i];

		if( curStage->ignore ) continue;

		if( !prevStage )
		{
			// no previous stage to merge with
			prevStage = curStage;
			continue;
		}

		if( !R_MergeShaderStages( prevStage, curStage ))
		{
			// couldn't merge the stages.
			prevStage = curStage;
			continue;
		}

		// merge with previous stage and ignore it
		Mem_Copy( prevStage->bundles[prevStage->numBundles++], curStage->bundles[0], sizeof( stageBundle_t ));
		curStage->ignore = true;
	}

	// make sure texEnv is valid (left-over from R_FinishShader)
	for( i = 0; i < shader->numStages; i++ )
	{
		if( shader->stages[i]->ignore ) continue;
		if( shader->stages[i]->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE )
			continue;
		if( shader->stages[i]->bundles[0]->texEnv != GL_REPLACE && shader->stages[i]->bundles[0]->texEnv != GL_MODULATE )
			shader->stages[i]->bundles[0]->texEnv = GL_MODULATE;
	}

	// check for constant expressions
	if( !shader->numstatements ) return;

	for( i = 0, op = shader->statements; i < shader->numstatements; i++, op++ )
	{
		if( op->opType != OP_TYPE_TABLE )
		{
			if( op->a < EXP_REGISTER_NUM_PREDEFINED || op->b < EXP_REGISTER_NUM_PREDEFINED )
				break;
		}
		else
		{
			if( op->b < EXP_REGISTER_NUM_PREDEFINED )
				break;
		}
	}

	if( i != shader->numstatements ) return; // something references a variable

	// evaluate all the registers
	for( i = 0, op = shader->statements; i < shader->numstatements; i++, op++ )
	{
		switch( op->opType )
		{
		case OP_TYPE_MULTIPLY:
			registers[op->c] = registers[op->a] * registers[op->b];
			break;
		case OP_TYPE_DIVIDE:
			if( registers[op->b] == 0.0 )
			{
				registers[op->c] = 0.0;
				break;
			}
			registers[op->c] = registers[op->a] / registers[op->b];
			break;
		case OP_TYPE_MOD:
			if( registers[op->b] == 0.0 )
			{
				registers[op->c] = 0.0;
				break;
			}
			registers[op->c] = (int)registers[op->a] % (int)registers[op->b];
			break;
		case OP_TYPE_ADD:
			registers[op->c] = registers[op->a] + registers[op->b];
			break;
		case OP_TYPE_SUBTRACT:
			registers[op->c] = registers[op->a] - registers[op->b];
			break;
		case OP_TYPE_GREATER:
			registers[op->c] = registers[op->a] > registers[op->b];
			break;
		case OP_TYPE_LESS:
			registers[op->c] = registers[op->a] < registers[op->b];
			break;
		case OP_TYPE_GEQUAL:
			registers[op->c] = registers[op->a] >= registers[op->b];
			break;
		case OP_TYPE_LEQUAL:
			registers[op->c] = registers[op->a] <= registers[op->b];
			break;
		case OP_TYPE_EQUAL:
			registers[op->c] = registers[op->a] == registers[op->b];
			break;
		case OP_TYPE_NOTEQUAL:
			registers[op->c] = registers[op->a] != registers[op->b];
			break;
		case OP_TYPE_AND:
			registers[op->c] = registers[op->a] && registers[op->b];
			break;
		case OP_TYPE_OR:
			registers[op->c] = registers[op->a] || registers[op->b];
			break;
		case OP_TYPE_TABLE:
			registers[op->c] = R_LookupTable( op->a, registers[op->b] );
			break;
		}
	}

	// we don't need to evaluate the registers during rendering, except for development purposes
	shader->constantExpressions = true;
}

static void R_ShaderTouchImages( ref_shader_t *shader, bool free_unused )
{
	int		i, j, k;
	int		c_total = 0;
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	texture_t		*texture;

	Com_Assert( shader == NULL );

	for( i = 0; i < shader->numStages; i++ )
	{
		stage = shader->stages[i];
		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

 			// FIXME: implement
			//if( free_unused && bundle->flags & STAGEBUNDLE_VIDEOMAP )
			//	CIN_StopCinematic( bundle->cinematicHandle );

			for( k = 0; k < bundle->numTextures; k++ )
			{
				// prolonge registration for all shader textures
				texture = bundle->textures[k];

				if( !texture || !texture->name[0] ) continue;
				if( free_unused && texture->touchFrame != registration_sequence )
					R_FreeImage( texture );
				else texture->touchFrame = registration_sequence;
				c_total++; // just for debug
			}
		}
	}

	// also update skybox if present
	if( shader->flags & SHADER_SKYPARMS )
	{
		for( i = 0; i < 6; i++ )
		{
			texture = shader->skyParms.farBox[i];
			if( texture && texture->name[0] )
			{
				if( free_unused && texture->touchFrame != registration_sequence )
					R_FreeImage( texture );
				else texture->touchFrame = registration_sequence;
			}
			texture = shader->skyParms.nearBox[i];
			if( texture && texture->texnum )
			{
				if( free_unused && texture->touchFrame != registration_sequence )
					R_FreeImage( texture );
				else texture->touchFrame = registration_sequence;
			}
			c_total++; // just for debug
		}
	}
}

/*
=================
R_LoadShader
=================
*/
ref_shader_t *R_LoadShader( ref_shader_t *newShader )
{
	ref_shader_t	*shader;
	uint		hashKey;
	int		i, j;

	// make sure the shader is valid and set all the unset parameters
	R_FinishShader( newShader );

	// try to merge multiple stages for multitexturing
	R_OptimizeShader( newShader );

	// copy the shader
	shader = &r_shaders[newShader->shadernum];
	Mem_Copy( shader, newShader, sizeof( ref_shader_t ));
	shader->mempool = Mem_AllocPool( shader->name );
	shader->numStages = 0;

	// allocate and copy the stages
	for( i = 0; i < newShader->numStages; i++ )
	{
		if( newShader->stages[i]->ignore )
			continue;

		shader->stages[shader->numStages] = Mem_Alloc( shader->mempool, sizeof( shaderStage_t ));
		Mem_Copy( shader->stages[shader->numStages], newShader->stages[i], sizeof( shaderStage_t ));

		// allocate and copy the bundles
		for( j = 0; j < shader->stages[shader->numStages]->numBundles; j++ )
		{
			shader->stages[shader->numStages]->bundles[j] = Mem_Alloc( shader->mempool, sizeof( stageBundle_t ));
			Mem_Copy( shader->stages[shader->numStages]->bundles[j], newShader->stages[i]->bundles[j], sizeof( stageBundle_t ));
		}
		shader->numStages++;
	}

	// allocate and copy the expression ops
	shader->statements = Mem_Alloc( shader->mempool, shader->numstatements * sizeof( statement_t ));
	Mem_Copy( shader->statements, newShader->statements, shader->numstatements * sizeof( statement_t ));

	// Allocate and copy the expression registers
	shader->expressions = Mem_Alloc( shader->mempool, shader->numRegisters * sizeof( float ));
	Mem_Copy( shader->expressions, newShader->expressions, shader->numRegisters * sizeof( float ));

	shader->touchFrame = registration_sequence;
	R_ShaderTouchImages( shader, false );

	// add to hash table
	hashKey = Com_HashKey( shader->name, SHADERS_HASH_SIZE );
	shader->nextHash = r_shadersHash[hashKey];
	r_shadersHash[hashKey] = shader;

	return shader;
}

/*
=================
R_FindShader
=================
*/
ref_shader_t *R_FindShader( const char *name, int shaderType, uint surfaceParm )
{
	ref_shader_t	*shader;
	ref_script_t	*shaderScript;
	uint		hashKey;

	if( !name || !name[0] ) Host_Error( "R_FindShader: NULL shader name\n" );

	// see if already loaded
	hashKey = Com_HashKey( name, SHADERS_HASH_SIZE );

	for( shader = r_shadersHash[hashKey]; shader; shader = shader->nextHash )
	{
		if( shader->type != shaderType || shader->surfaceParm != surfaceParm )
			continue;

		if( !com.stricmp( shader->name, name ))
		{
			// prolonge registration
			shader->touchFrame = registration_sequence;
			R_ShaderTouchImages( shader, false );
			return shader;
		}
	}

	// see if there's a script for this shader
	for( shaderScript = r_shaderScriptsHash[hashKey]; shaderScript; shaderScript = shaderScript->nextHash )
	{
		if( !com.stricmp( shaderScript->name, name ))
		{
			if( shaderScript->type == -1 ) break; // not initialized
			if( shaderScript->type == shaderType ) break;
		}
	}
 
 	// create the shader
	shader = R_CreateShader( name, shaderType, surfaceParm, shaderScript );

	// load it in
	return R_LoadShader( shader );
}

void R_SetInternalMap( texture_t *mipTex )
{
	// never replace with NULL
	if( mipTex ) r_internalMiptex = mipTex;
}

/*
=================
R_EvaluateRegisters
=================
*/
void R_EvaluateRegisters( ref_shader_t *shader, float time, const float *entityParms, const float *globalParms )
{
	float		*registers = shader->expressions;
	statement_t	*op;
	int		i;

	if( shader->constantExpressions ) return;

	// update the predefined registers
	registers[EXP_REGISTER_ONE] = 1.0;
	registers[EXP_REGISTER_ZERO] = 0.0;
	registers[EXP_REGISTER_TIME] = time;
	registers[EXP_REGISTER_PARM0] = entityParms[0];
	registers[EXP_REGISTER_PARM1] = entityParms[1];
	registers[EXP_REGISTER_PARM2] = entityParms[2];
	registers[EXP_REGISTER_PARM3] = entityParms[3];
	registers[EXP_REGISTER_PARM4] = entityParms[4];
	registers[EXP_REGISTER_PARM5] = entityParms[5];
	registers[EXP_REGISTER_PARM6] = entityParms[6];
	registers[EXP_REGISTER_PARM7] = entityParms[7];
	registers[EXP_REGISTER_GLOBAL0] = globalParms[0];
	registers[EXP_REGISTER_GLOBAL1] = globalParms[1];
	registers[EXP_REGISTER_GLOBAL2] = globalParms[2];
	registers[EXP_REGISTER_GLOBAL3] = globalParms[3];
	registers[EXP_REGISTER_GLOBAL4] = globalParms[4];
	registers[EXP_REGISTER_GLOBAL5] = globalParms[5];
	registers[EXP_REGISTER_GLOBAL6] = globalParms[6];
	registers[EXP_REGISTER_GLOBAL7] = globalParms[7];

	// evaluate all the registers
	for( i = 0, op = shader->statements; i < shader->numstatements; i++, op++ )
	{
		switch( op->opType )
		{
		case OP_TYPE_MULTIPLY:
			registers[op->c] = registers[op->a] * registers[op->b];
			break;
		case OP_TYPE_DIVIDE:
			if( registers[op->b] == 0.0 )
				Host_Error( "R_EvaluateRegisters: division by zero\n" );
			registers[op->c] = registers[op->a] / registers[op->b];
			break;
		case OP_TYPE_MOD:
			if( registers[op->b] == 0.0 )
				Host_Error( "R_EvaluateRegisters: mod division by zero\n" );
			registers[op->c] = (int)registers[op->a] % (int)registers[op->b];
			break;
		case OP_TYPE_ADD:
			registers[op->c] = registers[op->a] + registers[op->b];
			break;
		case OP_TYPE_SUBTRACT:
			registers[op->c] = registers[op->a] - registers[op->b];
			break;
		case OP_TYPE_GREATER:
			registers[op->c] = registers[op->a] > registers[op->b];
			break;
		case OP_TYPE_LESS:
			registers[op->c] = registers[op->a] < registers[op->b];
			break;
		case OP_TYPE_GEQUAL:
			registers[op->c] = registers[op->a] >= registers[op->b];
			break;
		case OP_TYPE_LEQUAL:
			registers[op->c] = registers[op->a] <= registers[op->b];
			break;
		case OP_TYPE_EQUAL:
			registers[op->c] = registers[op->a] == registers[op->b];
			break;
		case OP_TYPE_NOTEQUAL:
			registers[op->c] = registers[op->a] != registers[op->b];
			break;
		case OP_TYPE_AND:
			registers[op->c] = registers[op->a] && registers[op->b];
			break;
		case OP_TYPE_OR:
			registers[op->c] = registers[op->a] || registers[op->b];
			break;
		case OP_TYPE_TABLE:
			registers[op->c] = R_LookupTable(op->a, registers[op->b]);
			break;
		default:
			Host_Error( "R_EvaluateRegisters: bad opType (%i)", op->opType );
		}
	}
}

/*
=================
R_ModRegisterShaders

update shader and associated textures
=================
*/
void R_ModRegisterShaders( rmodel_t *mod )
{
	ref_shader_t	*shader;
	int		i;

	if( !mod || !mod->name[0] )
		return;

	for( i = 0; i < mod->numShaders; i++ )
	{
		shader = mod->shaders[i];
		if( !shader || !shader->name[0] ) continue;
		shader = R_FindShader( shader->name, shader->type, shader->surfaceParm );
	}
}

static void R_FreeShader( ref_shader_t *shader )
{
	uint		hash;
	ref_shader_t	*cur;
	ref_shader_t	**prev;
	
	Com_Assert( shader == NULL );
	
	// free uinque shader images only
	R_ShaderTouchImages( shader, true );

	// remove from hash table
	hash = Com_HashKey( shader->name, SHADERS_HASH_SIZE );
	prev = &r_shadersHash[hash];

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

	// free stages
	Mem_FreePool( &shader->mempool );
	Mem_Set( shader, 0, sizeof( *shader ));
}

void R_ShaderFreeUnused( void )
{
	ref_shader_t	*shader;
	int		i;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->name[0] ) continue;
		
		// used this sequence
		if( shader->touchFrame == registration_sequence ) continue;
		if( shader->flags & SHADER_STATIC ) continue;
		R_FreeShader( shader );
	}
}

/*
=================
R_CreateBuiltInShaders
=================
*/
static void R_CreateBuiltInShaders( void )
{
	ref_shader_t	*shader;
	int		i;

	// default shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<default>", sizeof( shader->name ));
	shader->type = SHADER_TEXTURE;
	shader->flags = SHADER_STATIC;
	shader->surfaceParm = SURF_NOLIGHTMAP;
	shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
	shader->stages[0]->bundles[0]->numTextures++;
	shader->stages[0]->numBundles++;
	shader->numStages++;

	tr.defaultShader = R_LoadShader( shader );

	// lightmap shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<lightmap>", sizeof( shader->name ));
	shader->type = SHADER_TEXTURE;
	shader->flags = SHADER_HASLIGHTMAP|SHADER_STATIC;
	shader->stages[0]->bundles[0]->texType = TEX_LIGHTMAP;
	shader->stages[0]->numBundles++;
	shader->numStages++;

	tr.lightmapShader = R_LoadShader( shader );

	// skybox shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<skybox>", sizeof( shader->name ));
	shader->type = SHADER_SKY;
	shader->flags = SHADER_SKYPARMS|SHADER_STATIC;
	for( i = 0; i < 6; i++ )
		shader->skyParms.farBox[i] = r_skyTexture;
	shader->skyParms.cloudHeight = 128.0f;
	tr.skyboxShader = R_LoadShader( shader );
	
	// particle shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<particle>", sizeof( shader->name ));
	shader->type = SHADER_SPRITE;
	shader->flags = SHADER_STATIC;
	shader->surfaceParm = SURF_NOLIGHTMAP;
	shader->stages[0]->bundles[0]->textures[0] = r_particleTexture;
	shader->stages[0]->blendFunc.src = GL_DST_COLOR;
	shader->stages[0]->blendFunc.dst = GL_SRC_ALPHA;
	shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC|SHADERSTAGE_RGBGEN;
	shader->stages[0]->rgbGen.type = RGBGEN_VERTEX;
	shader->stages[0]->bundles[0]->numTextures++;
	shader->stages[0]->numBundles++;
	shader->numStages++;

	tr.particleShader = R_LoadShader( shader );
}

/*
=================
R_ShaderList_f
=================
*/
void R_ShaderList_f( void )
{
	ref_shader_t	*shader;
	int		i, j;
	int		passes;
	int		shaderCount;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = shaderCount = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->shadernum ) continue;
		for( passes = j = 0; j < shader->numStages; j++ )
			passes += shader->stages[j]->numBundles;

		Msg( "%i/%i ", passes, shader->numStages );

		if( shader->flags & SHADER_EXTERNAL )
			Msg("E ");
		else Msg("  ");

		switch( shader->type )
		{
		case SHADER_SKY:
			Msg( "sky " );
			break;
		case SHADER_TEXTURE:
			Msg( "bsp " );
			break;
		case SHADER_STUDIO:
			Msg( "mdl " );
			break;
		case SHADER_SPRITE:
			Msg( "spr " );
			break;
		case SHADER_NOMIP:
			Msg( "pic " );
			break;
		case SHADER_GENERIC:
			Msg( "gen " );
			break;
		}

		if( shader->surfaceParm )
			Msg( "%02X ", shader->surfaceParm );
		else Msg("   ");

		Msg( "%2i ", shader->sort );
		Msg( ": %s%s\n", shader->name, (shader->flags & SHADER_DEFAULTED) ? " (DEFAULTED)" : "" );
		shaderCount++;
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total shaders\n", shaderCount );
	Msg( "\n" );
}

/*
=================
R_InitShaders
=================
*/
void R_InitShaders( void )
{
	script_t	*script;
	search_t	*t;
	int	i;

	MsgDev( D_NOTE, "R_InitShaders()\n" );

	r_shaderpool = Mem_AllocPool( "Shader Zone" );
	t = FS_Search( "scripts/*.shader", true );
	if( !t ) MsgDev( D_WARN, "no shader files found!\n");

	// Load them
	for( i = 0; t && i < t->numfilenames; i++ )
	{
		script = Com_OpenScript( t->filenames[i], NULL, 0 );
		if( !script )
		{
			MsgDev( D_ERROR, "Couldn't load '%s'\n", t->filenames[i] );
			continue;
		}

		// parse this file
		R_ParseShaderFile( script, t->filenames[i] );
		Com_CloseScript( script );
	}
	if( t ) Mem_Free( t ); // free search

	// create built-in shaders
	R_CreateBuiltInShaders();
	r_internalMiptex = r_defaultTexture;
}

/*
=================
R_ShutdownShaders
=================
*/
void R_ShutdownShaders( void )
{
	ref_shader_t	*shader;
	int		i;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->shadernum ) continue;	// already freed
		R_FreeShader( shader );
	}

	Mem_FreePool( &r_shaderpool ); // free all data allocated by shaders
	Mem_Set( r_shaderScriptsHash, 0, sizeof( r_shaderScriptsHash ));
	Mem_Set( r_shadersHash, 0, sizeof( r_shadersHash ));
	Mem_Set( r_shaders, 0, sizeof( r_shaders ));
	r_numShaders = 0;
}
