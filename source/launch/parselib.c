//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         parselib.c - script files parser
//=======================================================================

#include "launch.h"
#include "parselib.h"

// default punctuation table
static punctuation_t ps_punctuationsTable[] = 
{
{">>=",	PT_RSHIFT_ASSIGN	},
{"<<=",	PT_LSHIFT_ASSIGN	},
{"...",	PT_PARAMETERS	},
{"##",	PT_PRECOMPILER_MERGE},
{"&&",	PT_LOGIC_AND	},
{"||",	PT_LOGIC_OR	},
{">=",	PT_LOGIC_GEQUAL	},
{"<=",	PT_LOGIC_LEQUAL	},
{"==",	PT_LOGIC_EQUAL	},
{"!=",	PT_LOGIC_NOTEQUAL	},
{"*=",	PT_MUL_ASSIGN	},
{"/=",	PT_DIV_ASSIGN	},
{"%=",	PT_MOD_ASSIGN	},
{"+=",	PT_ADD_ASSIGN	},
{"-=",	PT_SUB_ASSIGN	},
{"++",	PT_INCREMENT	},
{"--",	PT_DECREMENT	},
{"&=",	PT_BINARY_AND_ASSIGN},
{"|=",	PT_BINARY_OR_ASSIGN	},
{"^=",	PT_BINARY_XOR_ASSIGN},
{">>",	PT_RSHIFT		},
{"<<",	PT_LSHIFT		},
{"->",	PT_POINTER_REFERENCE},
{"::",	PT_CPP_1		},
{".*",	PT_CPP_2		},
{"*",	PT_MUL		},
{"/",	PT_DIV		},
{"%",	PT_MOD		},
{"+",	PT_ADD		},
{"-",	PT_SUB		},
{"=",	PT_ASSIGN		},
{"&",	PT_BINARY_AND	},
{"|",	PT_BINARY_OR	},
{"^",	PT_BINARY_XOR	},
{"~",	PT_BINARY_NOT	},
{"!",	PT_LOGIC_NOT	},
{">",	PT_LOGIC_GREATER	},
{"<",	PT_LOGIC_LESS	},
{".",	PT_REFERENCE	},
{":",	PT_COLON		},
{",",	PT_COMMA		},
{";",	PT_SEMICOLON	},
{"?",	PT_QUESTION_MARK	},
{"{",	PT_BRACE_OPEN	},
{"}",	PT_BRACE_CLOSE	},
{"[",	PT_BRACKET_OPEN	},
{"]",	PT_BRACKET_CLOSE	},
{"(",	PT_PARENTHESIS_OPEN	},
{")",	PT_PARENTHESIS_CLOSE},
{"#",	PT_PRECOMPILER	},
{"$",	PT_DOLLAR		},
{"\\",	PT_BACKSLASH	},
{NULL,	0}
};

/*
=================
PS_NumberValue
=================
*/
static void PS_NumberValue( token_t *token )
{
	char	*string = token->string;
	double	fraction = 0.1, power = 1.0;
	bool	negative = false;
	int	i, exponent = 0;

	token->floatValue = 0.0;
	token->integerValue = 0;

	if( token->type != TT_NUMBER )
		return;

	// if a decimal number
	if( token->subType & NT_DECIMAL )
	{
		// if a floating point number
		if( token->subType & NT_FLOAT )
		{
			while( *string && *string != '.' && *string != 'e' && *string != 'E' )
				token->floatValue = token->floatValue * 10.0 + (double)(*string++ - '0');

			if( *string == '.' )
			{
				string++;
				while( *string && *string != 'e' && *string != 'E' )
				{
					token->floatValue = token->floatValue + (double)(*string++ - '0') * fraction;
					fraction *= 0.1;
				}
			}

			if( *string == 'e' || *string == 'E' )
			{
				string++;

				if( *string == '+' )
				{
					string++;
					negative = false;
				}
				else if( *string == '-' )
				{
					string++;
					negative = true;
				}

				while( *string ) exponent = exponent * 10 + (*string++ - '0');

				for( i = 0; i < exponent; i++ ) power *= 10.0;

				if( negative ) token->floatValue /= power;
				else token->floatValue *= power;
			}
			token->integerValue = (uint)token->floatValue;
			return;
		}

		// if an integer number
		if( token->subType & NT_INTEGER )
		{
			while( *string ) token->integerValue = token->integerValue * 10 + (*string++ - '0');
			token->floatValue = (double)token->integerValue;
			return;
		}
		return;
	}

	// if a binary number
	if( token->subType & NT_BINARY )
	{
		string += 2;
		while( *string ) token->integerValue = (token->integerValue<<1) + (*string++ - '0');
		token->floatValue = (double)token->integerValue;
		return;
	}

	// if an octal number
	if( token->subType & NT_OCTAL )
	{
		string += 1;
		while( *string ) token->integerValue = (token->integerValue<<3) + (*string++ - '0');
		token->floatValue = (double)token->integerValue;
		return;
	}

	// if a hexadecimal number
	if( token->subType & NT_HEXADECIMAL )
	{
		string += 2;
		while( *string )
		{
			if( *string >= 'a' && *string <= 'f' )
				token->integerValue = (token->integerValue<<4) + (*string++ - 'a' + 10);
			else if( *string >= 'A' && *string <= 'F' )
				token->integerValue = (token->integerValue<<4) + (*string++ - 'A' + 10);
			else token->integerValue = (token->integerValue<<4) + (*string++ - '0');
		}
		token->floatValue = (double)token->integerValue;
		return;
	}
}

/*
=================
PS_ReadWhiteSpace
=================
*/
static bool PS_ReadWhiteSpace( script_t *script, scFlags_t flags )
{
	char	*text;
	int	line;
	bool	hasNewLines = false;

	// backup text and line
	text = script->text;
	line = script->line;

	while( 1 )
	{
		// skip whitespace
		while( *script->text <= ' ' )
		{
			if( !*script->text )
			{
				script->text = NULL;
				return false;
			}
			if( *script->text == '\n' )
			{
				script->line++;
				hasNewLines = true;
			}
			script->text++;
		}

		// if newlines are not allowed, restore text and line
		if( hasNewLines && !(flags & SC_ALLOW_NEWLINES))
		{
			script->text = text;
			script->line = line;
			return false;
		}

		// QuArk comments: ;TX1, //TX1, #TX1
		if( flags & SC_QUARK_COMMAND && ( *script->text == ';' || *script->text == '#' ))
		{
			if( script->text[1] == 'T' && script->text[2] == 'X' )
				GI.TXcommand = script->text[3]; // QuArK TX#"-style comment
		}

		// skip // comments
		if( *script->text == '/' && script->text[1] == '/' )
		{
			if( flags & SC_QUARK_COMMAND )
			{
				if( *script->text == '/' ) *script->text++;
				if( script->text[1] == 'T' && script->text[2] == 'X' )
					GI.TXcommand = script->text[3]; // QuArK TX#"-style comment
			}
			while( *script->text && *script->text != '\n' )
				script->text++;
			continue;
		}

		// skip /* */ comments
		if( *script->text == '/' && script->text[1] == '*' )
		{
			script->text += 2;
			while( *script->text && (*script->text != '*' || script->text[1] != '/' ))
			{
				if( *script->text == '\n' )
					script->line++;
				script->text++;
			}
			if( *script->text ) script->text += 2;
			continue;
		}

		// an actual token
		break;
	}
	return true;
}

/*
=================
PS_ReadEscapeChar
=================
*/
static bool PS_ReadEscapeChar( script_t *script, scFlags_t flags, char *ch )
{
	int	value;

	script->text++;

	switch( *script->text )
	{
	case 'a':
		*ch = '\a';
		break;
	case 'b':
		*ch = '\b';
		break;
	case 'f':
		*ch = '\f';
		break;
	case 'n':
		*ch = '\n';
		break;
	case 'r':
		*ch = '\r';
		break;
	case 't':
		*ch = '\t';
		break;
	case 'v':
		*ch = '\v';
		break;
	case '\"':
		*ch = '\"';
		break;
	case '\'':
		*ch = '\'';
		break;
	case '\\':
		*ch = '\\';
		break;
	case '\?':
		*ch = '\?';
		break;
	case 'x':
		script->text++;

		for( value = 0; ; script->text++ )
		{
			if( *script->text >= 'a' && *script->text <= 'f' )
				value = (value<<4) + (*script->text - 'a' + 10);
			else if( *script->text >= 'A' && *script->text <= 'F' )
				value = (value<<4) + (*script->text - 'A' + 10 );
			else if( *script->text >= '0' && *script->text <= '9' )
				value = (value<<4) + (*script->text - '0' );
			else break;
		}

		script->text--;

		if( value > 0xFF )
		{
			PS_ScriptError( script, flags, "too large value in escape character" );
			return false;
		}
		*ch = value;
		break;
	default:
		if( *script->text < '0' || *script->text > '9' )
		{
			PS_ScriptError( script, flags, "unknown escape character" );
			return false;
		}

		for( value = 0; ; script->text++ )
		{
			if( *script->text >= '0' && *script->text <= '9' )
				value = value * 10 + (*script->text - '0');
			else break;
		}

		script->text--;

		if( value > 0xFF )
		{
			PS_ScriptError( script, flags, "too large value in escape character" );
			return false;
		}

		*ch = value;
		break;
	}
	script->text++;

	return true;
}

/*
=================
PS_ReadGeneric
=================
*/
static bool PS_ReadGeneric( script_t *script, scFlags_t flags, token_t *token )
{
	token->type = TT_GENERIC;
	token->subType = 0;
	token->line = script->line;

	while( 1 )
	{
		if( *script->text <= ' ' ) break;
		if( token->length == MAX_TOKEN_LENGTH - 1 )
		{
			PS_ScriptError( script, flags, "string longer than %i symbols", MAX_TOKEN_LENGTH );
			return false;
		}
		token->string[token->length++] = *script->text++;
	}

	token->string[token->length] = 0;
	PS_NumberValue( token );

	return true;
}

/*
=================
PS_ReadString
=================
*/
static bool PS_ReadString( script_t *script, scFlags_t flags, token_t *token )
{
	char	*text;
	int	line;

	token->type = TT_STRING;
	token->subType = 0;

	token->line = script->line;
	script->text++;

	while( 1 )
	{
		if( !*script->text )
		{
			PS_ScriptError( script, flags, "missing trailing quote" );
			return false;
		}

		if( *script->text == '\n' )
		{
			PS_ScriptError( script, flags, "newline inside string" );
			return false;
		}

		if( *script->text == '\"' )
		{
			script->text++;

			if( !(flags & SC_ALLOW_STRINGCONCAT))
				break;

			text = script->text;
			line = script->line;

			if( PS_ReadWhiteSpace( script, flags ))
			{
				if( *script->text == '\"' )
				{
					script->text++;
					continue;
				}
			}

			script->text = text;
			script->line = line;
			break;
		}

		if( token->length == MAX_TOKEN_LENGTH - 1 )
		{
			PS_ScriptError( script, flags, "string longer than %i symbols", MAX_TOKEN_LENGTH );
			return false;
		}

		if(( flags & SC_ALLOW_ESCAPECHARS) && *script->text == '\\' )
		{
			if( !PS_ReadEscapeChar( script, flags, &token->string[token->length] ))
				return false;
			token->length++;
			continue;
		}
		token->string[token->length++] = *script->text++;
	}

	token->string[token->length] = 0;
	PS_NumberValue( token );

	return true;
}

/*
=================
PS_ReadLiteral
=================
*/
static bool PS_ReadLiteral( script_t *script, scFlags_t flags, token_t *token )
{
	char	*text;
	int	line;

	token->type = TT_LITERAL;
	token->line = script->line;
	token->subType = 0;
	script->text++;

	while( 1 )
	{
		if( !*script->text )
		{
			PS_ScriptError( script, flags, "missing trailing quote" );
			return false;
		}

		if( *script->text == '\n' )
		{
			PS_ScriptError( script, flags, "newline inside literal" );
			return false;
		}

		if( *script->text == '\'' )
		{
			script->text++;

			if(!(flags & SC_ALLOW_STRINGCONCAT))
				break;

			text = script->text;
			line = script->line;

			if( PS_ReadWhiteSpace( script, flags ))
			{
				if( *script->text == '\'' )
				{
					script->text++;
					continue;
				}
			}

			script->text = text;
			script->line = line;
			break;
		}

		if( token->length == MAX_TOKEN_LENGTH - 1 )
		{
			PS_ScriptError( script, flags, "literal longer than %i symbols", MAX_TOKEN_LENGTH );
			return false;
		}

		if((flags & SC_ALLOW_ESCAPECHARS) && *script->text == '\\' )
		{
			if( !PS_ReadEscapeChar( script, flags, &token->string[token->length] ))
				return false;
			token->length++;
			continue;
		}
		token->string[token->length++] = *script->text++;
	}

	token->string[token->length] = 0;
	PS_NumberValue( token );

	return true;
}

/*
=================
PS_ReadNumber
=================
*/
static bool PS_ReadNumber( script_t *script, scFlags_t flags, token_t *token )
{
	bool	hasDot = false;
	int	c;

	token->type = TT_NUMBER;
	token->subType = 0;

	token->line = script->line;

	if( *script->text == '0' && script->text[1] != '.' )
	{
		if( script->text[1] == 'b' || script->text[1] == 'B' )
		{
			token->string[token->length++] = *script->text++;
			token->string[token->length++] = *script->text++;

			while( 1 )
			{
				c = *script->text;

				if( c < '0' || c > '1' )
					break;

				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "binary number longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = *script->text++;
			}
			token->subType |= (NT_BINARY|NT_INTEGER);
		}
		else if( script->text[1] == 'x' || script->text[1] == 'X' )
		{
			token->string[token->length++] = *script->text++;
			token->string[token->length++] = *script->text++;

			while( 1 )
			{
				c = *script->text;

				if((c < 'a' || c > 'f') && (c < 'A' || c > 'F') && (c < '0' || c > '9'))
					break;

				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "hexadecimal number longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = *script->text++;
			}

			token->subType |= (NT_HEXADECIMAL|NT_INTEGER);
		}
		else
		{
			token->string[token->length++] = *script->text++;

			while( 1 )
			{
				c = *script->text;

				if( c < '0' || c > '7' )
					break;

				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "octal number longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = *script->text++;
			}
			token->subType |= (NT_OCTAL|NT_INTEGER);
		}

		token->string[token->length] = 0;
		PS_NumberValue( token );
		return true;
	}

	while( 1 )
	{
		c = *script->text;

		if( c == '.' )
		{
			if( hasDot ) break;
			hasDot = true;
		}
		else if( c < '0' || c > '9' )
			break;

		if( token->length == MAX_TOKEN_LENGTH - 1 )
		{
			PS_ScriptError( script, flags, "number longer than %i symbols", MAX_TOKEN_LENGTH );
			return false;
		}
		token->string[token->length++] = *script->text++;
	}

	if( hasDot || (*script->text == 'e' || *script->text == 'E'))
	{
		token->subType |= (NT_DECIMAL|NT_FLOAT);

		if( *script->text == 'e' || *script->text == 'E' )
		{
			if( token->length == MAX_TOKEN_LENGTH - 1 )
			{
				PS_ScriptError( script, flags, "number longer than %i symbols", MAX_TOKEN_LENGTH );
				return false;
			}
			token->string[token->length++] = *script->text++;

			if( *script->text == '+' || *script->text == '-' )
			{
				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "number longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = *script->text++;
			}

			while( 1 )
			{
				c = *script->text;

				if( c < '0' || c > '9' ) break;

				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "number longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = *script->text++;
			}
		}

		if( *script->text == 'f' || *script->text == 'F' )
		{
			script->text++;
			token->subType |= NT_SINGLE;
		}
		else if( *script->text == 'l' || *script->text == 'L' )
		{
			script->text++;
			token->subType |= NT_EXTENDED;
		}
		else token->subType |= NT_DOUBLE;
	}
	else
	{
		token->subType |= (NT_DECIMAL|NT_INTEGER);

		if( *script->text == 'u' || *script->text == 'U' )
		{
			script->text++;
			token->subType |= NT_UNSIGNED;

			if( *script->text == 'l' || *script->text == 'L' )
			{
				script->text++;
				token->subType |= NT_LONG;
			}
		}
		else if( *script->text == 'l' || *script->text == 'L' )
		{
			script->text++;
			token->subType |= NT_LONG;

			if( *script->text == 'u' || *script->text == 'U' )
			{
				script->text++;
				token->subType |= NT_UNSIGNED;
			}
		}
	}

	token->string[token->length] = 0;
	PS_NumberValue( token );

	return true;
}

/*
=================
PS_ReadName
=================
*/
static bool PS_ReadName( script_t *script, scFlags_t flags, token_t *token )
{
	int	c;

	token->type = TT_NAME;
	token->subType = 0;

	token->line = script->line;

	while( 1 )
	{
		c = *script->text;

		if( flags & SC_ALLOW_PATHNAMES )
		{
			if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '_' 
			&& c != '/' && c != '\\' && c != ':' && c != '.' && c != '+' && c != '-')
				break;
		}
		else
		{
			if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '_')
				break;
		}

		if( token->length == MAX_TOKEN_LENGTH - 1 )
		{
			PS_ScriptError( script, flags, "name longer than %i symbols", MAX_TOKEN_LENGTH );
			return false;
		}
		token->string[token->length++] = *script->text++;
	}

	token->string[token->length] = 0;
	PS_NumberValue( token );

	return true;
}

/*
=================
PS_ReadPunctuation
=================
*/
static bool PS_ReadPunctuation( script_t *script, scFlags_t flags, token_t *token )
{
	punctuation_t	*punctuation;
	int		i, len;

	for( i = 0; script->punctuations[i].name; i++ )
	{
		punctuation = &script->punctuations[i];

		for( len = 0; punctuation->name[len] && script->text[len]; len++ )
		{
			if( punctuation->name[len] != script->text[len] )
				break;
		}

		if( !punctuation->name[len] )
		{
			script->text += len;
			token->type = TT_PUNCTUATION;
			token->subType = punctuation->type;
			token->line = script->line;

			for( i = 0; i < len; i++ )
			{
				if( token->length == MAX_TOKEN_LENGTH - 1 )
				{
					PS_ScriptError( script, flags, "punctuation longer than %i symbols", MAX_TOKEN_LENGTH );
					return false;
				}
				token->string[token->length++] = punctuation->name[i];
			}

			token->string[token->length] = 0;
			PS_NumberValue( token );
			return true;
		}
	}
	return false;
}

/*
=================
PS_ReadToken

Reads a token from the script
 =================
*/
bool PS_ReadToken( script_t *script, scFlags_t flags, token_t *token )
{
	// if there is a token available (from PS_FreeToken)
	if( script->tokenAvailable )
	{
		script->tokenAvailable = false;
		Mem_Copy( token, &script->token, sizeof( token_t ));
		return true;
	}

	// clear token
	token->type = TT_EMPTY;
	token->subType = 0;
	token->line = 1;
	token->string[0] = 0;
	token->length = 0;
	token->floatValue = 0.0;
	token->integerValue = 0;

	// make sure incoming data is valid
	if( !script->text ) return false;

	// skip whitespace and comments
	if( !PS_ReadWhiteSpace( script, flags ))
		return false;

	// if we just want to parse a generic string separated by spaces
	if( flags & SC_PARSE_GENERIC )
	{
		// if it is a string
		if( *script->text == '\"' )
		{
			if( PS_ReadString( script, flags, token ))
				return true;
		}
		// if it is a literal
		else if( *script->text == '\'' )
		{
			if( PS_ReadLiteral( script, flags, token ))
				return true;
		}
		// check for a generic string
		else if( PS_ReadGeneric( script, flags, token ))
			return true;
	}
	// If it is a string
	else if( *script->text == '\"' )
	{
		if( PS_ReadString( script, flags, token ))
			return true;
	}
	// if it is a literal
	else if( *script->text == '\'' )
	{
		if( PS_ReadLiteral( script, flags, token ))
			return true;
	}
	// if it is a number
	else if((*script->text >= '0' && *script->text <= '9') || (*script->text == '.' && (script->text[1] >= '0' && script->text[1] <= '9')))
	{
		if( PS_ReadNumber( script, flags, token ))
			return true;
	}
	// if it is a name
	else if((*script->text >= 'a' && *script->text <= 'z') || (*script->text >= 'A' && *script->text <= 'Z') || *script->text == '_')
	{
		if( PS_ReadName( script, flags, token ))
			return true;
	}
	// check for a path name if needed
	else if((flags & SC_ALLOW_PATHNAMES) && (*script->text == '/' || *script->text == '\\' || *script->text == ':' || *script->text == '.'))
	{
		if( PS_ReadName( script, flags, token ))
			return true;
	}
	// check for a punctuation
	else if( PS_ReadPunctuation( script, flags, token ))
		return true;

	// couldn't parse a token
	token->type = TT_EMPTY;
	token->subType = 0;
	token->line = 1;
	token->string[0] = 0;
	token->length = 0;
	token->floatValue = 0.0;
	token->integerValue = 0;

	PS_ScriptError( script, flags, "couldn't read token" );
	return false;
}

/*
=================
PS_FreeToken
=================
*/
void PS_FreeToken( script_t *script, token_t *token )
{
	script->tokenAvailable = true;
	Mem_Copy( &script->token, token, sizeof( token_t ));
}

/*
=================
PS_ReadDouble
=================
*/
bool PS_ReadDouble( script_t *script, scFlags_t flags, double *value )
{
	token_t	token;

	if( !PS_ReadToken( script, flags, &token ))
		return false;

	if( token.type == TT_PUNCTUATION && !com.stricmp( token.string, "-" ))
	{
		if( !PS_ReadToken( script, flags, &token ))
			return false;

		if( token.type != TT_NUMBER )
		{
			PS_ScriptError( script, flags, "expected float value, found '%s'", token.string );
			return false;
		}
		*value = -token.floatValue;
		return true;
	}

	if( token.type != TT_NUMBER )
	{
		PS_ScriptError( script, flags, "expected float value, found '%s'", token.string );
		return false;
	}
	*value = token.floatValue;
	return true;
}

/*
=================
PS_ReadFloat
=================
*/
bool PS_ReadFloat( script_t *script, scFlags_t flags, float *value )
{
	token_t	token;

	if( !PS_ReadToken( script, flags, &token ))
		return false;

	if( token.type == TT_PUNCTUATION && !com.stricmp( token.string, "-" ))
	{
		if( !PS_ReadToken( script, flags, &token ))
			return false;

		if( token.type != TT_NUMBER )
		{
			PS_ScriptError( script, flags, "expected float value, found '%s'", token.string );
			return false;
		}
		*value = -((float)token.floatValue);
		return true;
	}

	if( token.type != TT_NUMBER )
	{
		PS_ScriptError( script, flags, "expected float value, found '%s'", token.string );
		return false;
	}
	*value = (float)token.floatValue;
	return true;
}

/*
=================
PS_ReadUnsigned
=================
*/
bool PS_ReadUnsigned( script_t *script, scFlags_t flags, uint *value )
{
	token_t	token;

	if( !PS_ReadToken( script, flags, &token ))
		return false;

	if( token.type != TT_NUMBER || !(token.subType & NT_INTEGER ))
	{
		PS_ScriptError( script, flags, "expected integer value, found '%s'", token.string );
		return false;
	}
	*value = token.integerValue;
	return true;
}

/*
=================
PS_ReadInteger
=================
*/
bool PS_ReadInteger( script_t *script, scFlags_t flags, int *value )
{
	token_t	token;

	if( !PS_ReadToken( script, flags, &token ))
		return false;

	if( token.type == TT_PUNCTUATION && !com.stricmp( token.string, "-" ))
	{
		if( !PS_ReadToken( script, flags, &token ))
			return false;

		if( token.type != TT_NUMBER || !(token.subType & NT_INTEGER ))
		{
			PS_ScriptError( script, flags, "expected integer value, found '%s'", token.string );
			return false;
		}

		*value = -((int)token.integerValue);
		return true;
	}

	if( token.type != TT_NUMBER || !(token.subType & NT_INTEGER))
	{
		PS_ScriptError( script, flags, "expected integer value, found '%s'", token.string );
		return false;
	}

	*value = (int)token.integerValue;
	return true;
}

/*
=================
PS_SkipWhiteSpace

Skips until a printable character is found
=================
*/
void PS_SkipWhiteSpace( script_t *script )
{
	// make sure incoming data is valid
	if( !script->text ) return;

	while( 1 )
	{
		// skip whitespace
		while( *script->text <= ' ' )
		{
			if( !*script->text )
			{
				script->text = NULL;
				return;
			}

			if( *script->text == '\n' )
				script->line++;
			script->text++;
		}

		// skip // comments
		if( *script->text == '/' && script->text[1] == '/' )
		{
			while( *script->text && *script->text != '\n' )
				script->text++;
			continue;
		}

		// skip /* */ comments
		if( *script->text == '/' && script->text[1] == '*' )
		{
			script->text += 2;

			while( *script->text && (*script->text != '*' || script->text[1] != '/' ))
			{
				if( *script->text == '\n' )
					script->line++;
				script->text++;
			}
			if( *script->text ) script->text += 2;
			continue;
		}
		// an actual token
		break;
	}
}

/*
=================
PS_SkipRestOfLine

Skips until a new line is found
=================
*/
void PS_SkipRestOfLine( script_t *script )
{
	token_t	token;

	while( 1 )
	{
		if( !PS_ReadToken( script, 0, &token ))
			break;
	}
}

/*
=================
PS_SkipBracedSection

Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void PS_SkipBracedSection( script_t *script, int depth )
{
	token_t	token;

	do {
		if( !PS_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			break;

		if( token.type == TT_PUNCTUATION )
		{
			if( !com.stricmp( token.string, "{" ))
				depth++;
			else if( !com.stricmp( token.string, "}" ))
				depth--;
		}
	} while( depth );
}

/*
=================
PS_ScriptError
=================
*/
void PS_ScriptError( script_t *script, scFlags_t flags, const char *fmt, ... )
{
	string	errorstring;
	va_list	argPtr;

	if(!(flags & SC_PRINT_ERRORS)) return;

	va_start( argPtr, fmt );
	com_vsnprintf( errorstring, MAX_STRING, fmt, argPtr );
	va_end( argPtr );

	MsgDev( D_ERROR, "source '%s', line %i: %s\n", script->name, script->line, errorstring );
}

/*
=================
PS_ScriptWarning
=================
*/
void PS_ScriptWarning( script_t *script, scFlags_t flags, const char *fmt, ... )
{
	string	warnstring;
	va_list	argPtr;

	if(!(flags & SC_PRINT_WARNINGS)) return;

	va_start( argPtr, fmt );
	com.vsnprintf( warnstring, MAX_STRING, fmt, argPtr );
	va_end( argPtr );

	MsgDev( D_WARN, "source '%s', line %i: %s\n", script->name, script->line, warnstring );
}

/*
=================
PS_SetPunctuationsTable
=================
*/
void PS_SetPunctuationsTable( script_t *script, punctuation_t *punctuationsTable )
{
	if( punctuationsTable )
		script->punctuations = punctuationsTable;
	else script->punctuations = ps_punctuationsTable;
}

/*
=================
PS_ResetScript
=================
*/
void PS_ResetScript( script_t *script )
{
	script->text = script->buffer;
	script->line = 1;

	script->tokenAvailable = false;
	script->token.type = TT_EMPTY;
	script->token.subType = 0;
	script->token.line = 1;
	script->token.string[0] = 0;
	script->token.length = 0;
	script->token.floatValue = 0.0;
	script->token.integerValue = 0;
}

/*
=================
PS_EndOfScript
=================
*/
bool PS_EndOfScript( script_t *script )
{
	if( !script || !script->text )
		return true;
	return false;
}

/*
=================
PS_LoadScriptFile
=================
*/
script_t *PS_LoadScript( const char *filename, const char *buf, size_t size )
{
	script_t	*script;
	char	*buffer;

	// trying to get script from disk
	if( !buf || size <= 0 )
	{
		buf = FS_LoadFile( filename, &size );
		buffer = copystring2( Sys.scriptpool, buf );
		Mem_Free((char *)buf );		// can release now
	}
	else buffer = copystring2( Sys.scriptpool, buf );	// from memory
	if( !buffer || size <= 0 ) return NULL;		// let the caller handle this error

	// allocate the script_t
	script = Mem_Alloc( Sys.scriptpool, sizeof( script_t ));
	com.strncpy( script->name, filename, sizeof( script->name ));

	script->buffer = buffer;
	script->text = buffer;
	script->allocated = true;
	script->size = size;
	script->line = 1;

	script->punctuations = ps_punctuationsTable;
	script->tokenAvailable = false;
	script->token.type = TT_EMPTY;
	script->token.subType = 0;
	script->token.line = 1;
	script->token.string[0] = 0;
	script->token.length = 0;
	script->token.floatValue = 0.0;
	script->token.integerValue = 0;

	return script;
}

/*
=================
PS_FreeScript
=================
*/
void PS_FreeScript( script_t *script )
{
	if( !script )
	{
		MsgDev( D_WARN, "PS_FreeScript: trying to free NULL script\n" );
		return;
	}

	if( script->allocated )
		Mem_Free( script->buffer );
	Mem_Free( script );	// himself
}

/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/

typedef struct
{
	char	filename[1024];
	byte	*buffer; 
	byte	*script_p;
	byte	*end_p;
	int	line;
} old_script_t;

// max included scripts
#define	MAX_INCLUDES	32
#define	SC_EndOfScript( x )		SC_EndOfScript_( x, __LINE__ )
old_script_t	scriptstack[ MAX_INCLUDES ];
old_script_t	*script;

char token[MAX_MSGLEN]; // contains token info
static const char *saved_token;
static int saved_scriptline;

bool endofscript;
bool tokenready; // only true if UnGetToken was just called
bool SC_EndOfScript_ (bool newline, const int line );

/*
==============
SC_AddScriptToStack
==============
*/
bool SC_AddScriptToStack( const char *name, byte *buffer, int size )
{
	if (script == &scriptstack[MAX_INCLUDES - 1])
	{
		MsgDev( D_ERROR, "AddScriptToStack: script file limit exceeded %d\n", MAX_INCLUDES );
		return false;
	}
          if(!buffer || !size) return false;
	
	script++;
	com_strncpy(script->filename, name, sizeof(script->filename));
	script->buffer = buffer;
	script->line = com.com_scriptline = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	return true;
}

bool SC_LoadScript( const char *filename, char *buf, int size )
{
	int	result;
	string	scriptname;

	if(!buf || size <= 0)
	{
		com_strncpy( scriptname, filename, MAX_STRING );
		buf = FS_LoadFile( filename, &size );
	}
	else com_strncpy( scriptname, "*sc_buffer", MAX_STRING );

	script = scriptstack;
	result = SC_AddScriptToStack( scriptname, buf, size);

	endofscript = false;
	tokenready = false;
	return result;
}

bool SC_AddScript( const char *filename, char *buf, int size )
{
	string	scriptname;

	if(!buf || size <= 0)
	{
		com_strncpy( scriptname, filename, MAX_STRING );
		buf = FS_LoadFile( filename, &size );
	}
	else com_strncpy( scriptname, "*sc_buffer", MAX_STRING );
	return SC_AddScriptToStack( scriptname, buf, size );
}

void SC_ResetScript( void )
{
	// can parsing again
	script->line = com.com_scriptline = saved_scriptline = 1;
	script->script_p = script->buffer;
	saved_token = NULL;
}

/*
=================
SC_PushScript

FIXME: use scriptstack
=================
*/
void SC_PushScript( const char **data_p )
{
	saved_token = *data_p;
	saved_scriptline = com.com_scriptline;
}

/*
=================
SC_PopScript

FIXME: use scriptstack
=================
*/
void SC_PopScript( const char **data_p )
{
	*data_p = saved_token;
	com.com_scriptline = saved_scriptline;
}

/*
==============
SC_ReadToken
==============
*/
bool SC_ReadToken( bool newline )
{
	char	*token_p;
	int	c;

	if( tokenready )
	{
		// is a token allready waiting?
		tokenready = false;
		return true;
	}

	if (script->script_p >= script->end_p)
		return SC_EndOfScript (newline);

	
skip_whitespace:	// skip whitespace
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return SC_EndOfScript (newline);

		if (*script->script_p++ == '\n')
		{
			if (!newline) goto line_incomplete;
			com.com_scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p)
		return SC_EndOfScript (newline);

	// ; // comments
	if(*script->script_p == ';' || ( script->script_p[0] == '/' && script->script_p[1] == '/') )
	{
		if (!newline) goto line_incomplete;

		// ets++
		if (*script->script_p == '/') script->script_p++;
		while (*script->script_p++ != '\n')
		{
			if (script->script_p >= script->end_p)
				return SC_EndOfScript (newline);
		}
		com.com_scriptline = script->line++;
		goto skip_whitespace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*')
	{
		if (!newline) goto line_incomplete;

		script->script_p += 2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/')
		{
			if (script->script_p >= script->end_p)
				return SC_EndOfScript (newline);
			if (*script->script_p++ == '\n')
			{
				if (!newline) goto line_incomplete;
				com.com_scriptline = script->line++;
			}
		}
		script->script_p += 2;
		goto skip_whitespace;
	}

	// copy token
	token_p = token;
	c = script->script_p[0];
	
	// handle quoted strings specially
	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				MsgDev(D_WARN, "GetToken: Token too large on line %i\n", com.com_scriptline);
				break;
			}
			
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
		script->script_p++;
	}
	else if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' || c == '[' || c == ']')
	{
		// parse single characters
		*token_p++ = *script->script_p++;
	}
	else
	{
          	// parse regular word
		while ( *script->script_p > 32 )
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				MsgDev( D_WARN, "GetToken: Token too large on line %i\n",com.com_scriptline);
				break;
			}
			*token_p = c;
			*token_p++ = *script->script_p++;
			c = *script->script_p;

			if (script->script_p == script->end_p)
				break;
			if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' || c == ';' || c == '[' || c == ']')
				break;
		}
          }
	*token_p = 0; // cutoff other symbols

	// quake style include & default MSVC style
	if (!com_strcmp(token, "$include") || !com_strcmp(token, "#include"))
	{
		SC_ReadToken( false );
		SC_AddScript(token, NULL, 0 );
		return SC_ReadToken (newline);
	}
	return true;

line_incomplete:
	//invoke error
	return SC_EndOfScript( newline );
}

/*
==============
SC_ReadTokenSimple
==============
*/
bool SC_ReadTokenSimple( bool newline )
{
	char	*token_p;

	if( tokenready ) // is a token allready waiting?
	{
		tokenready = false;
		return true;
	}

	if( script->script_p >= script->end_p )
		return SC_EndOfScript( newline );

	
skip_whitespace:	// skip whitespace
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return SC_EndOfScript (newline);

		if (*script->script_p++ == '\n')
		{
			if( !newline ) return SC_EndOfScript( newline );
			com.com_scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p)
		return SC_EndOfScript (newline);

	// ; # // comments
	if( *script->script_p == ';' || *script->script_p == '#' || ( script->script_p[0] == '/' && script->script_p[1] == '/'))
	{
		if( !newline ) return SC_EndOfScript( newline );

		// ets+++
		if(*script->script_p == '/') script->script_p++;
		if( script->script_p[1] == 'T' && script->script_p[2] == 'X' )
		{
			GI.TXcommand = script->script_p[3];//TX#"-style comment
		}
		while (*script->script_p++ != '\n')
		{
			if (script->script_p >= script->end_p)
				return SC_EndOfScript (newline);
		}
		com.com_scriptline = script->line++;
		goto skip_whitespace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*')
	{
		if (!newline) return SC_EndOfScript( newline );

		script->script_p += 2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/')
		{
			if (script->script_p >= script->end_p)
				return SC_EndOfScript (newline);
			if (*script->script_p++ == '\n')
			{
				if( !newline ) return SC_EndOfScript( newline );
				com.com_scriptline = script->line++;
			}
		}
		script->script_p += 2;
		goto skip_whitespace;
	}

	// copy token
	token_p = token;

	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				MsgDev(D_WARN, "SC_ReadToken: Token too large on line %i\n", com.com_scriptline);
				break;
			}
			
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
		script->script_p++;
	}
	else // regular token
	{
		while ( *script->script_p > 32 && *script->script_p != ';')
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				MsgDev( D_WARN, "GetToken: Token too large on line %i\n",com.com_scriptline);
				break;
			}
		
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
          }
          
	*token_p = 0;

	// quake style include & default MSVC style
	if (!com_strcmp(token, "$include"))
	{
		SC_ReadTokenSimple(false);
		SC_AddScript(token, NULL, 0 );
		return SC_ReadTokenSimple(newline);
	}
	return true;
}
 
/*
==============
SC_EndOfScript
==============
*/
bool SC_EndOfScript_( bool newline, const int line )
{
	if( !newline ) 
	{
		com.com_scriptline = script->line;
		Sys_Break( "%s: line %i is incomplete (called at line %i)\n", script->filename, com.com_scriptline, line );
	}

	if(!com_strcmp(script->filename, "*sc_buffer"))
	{
		endofscript = true;
		return false;
	}

	// FIXME: stupid xash bug
	if(Mem_IsAllocated( script->buffer )) Mem_Free( script->buffer );
	else MsgDev( D_WARN, "attempt to free already freed script buffer '%s'\n", script->filename );

	// FIXME
	// script->buffer = NULL;

	if( script == scriptstack + 1 )
	{
		endofscript = true;
		return false;
	}

	script--;
	token[0] = 0; // clear last token
	com.com_scriptline = script->line;
	endofscript = true;

	return false;
}

/*
==============
SC_TokenAvailable
==============
*/
bool SC_TokenAvailable (void)
{
	char    *search_p;

	search_p = script->script_p;

	if(search_p >= script->end_p)
		return false;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == script->end_p)
			return false;
	}

	if (*search_p == ';')
		return false;
	return true;
}

/*
=================
SC_SkipWhiteSpace
=================
*/
const char *SC_SkipWhiteSpace( const char *data_p, bool *newline )
{
	int	c;

	while((c = *data_p) <= ' ')
	{
		if( !c ) return NULL;
		if( c == '\n' )
		{
			com.com_scriptline++;
			*newline = true;
		}
		data_p++;
	}
	return data_p;
}

/*
=================
SC_SkipBracedSection

Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void SC_SkipBracedSection( char **data_p, int depth )
{
	do
	{
		SC_ParseToken( data_p, true );
		if( !token[1] )
		{
			if( token[0] == '{' ) depth++;
			else if( token[0] == '}' ) depth--;
		}
	} while( depth && *data_p );
}

/*
==============
SC_ParseToken_Simple

Parse a token out of a string, behaving like the qwcl console
==============
*/
bool SC_ParseToken_Simple( const char **data_p )
{
	int len = 0;
	const char *data;

	token[0] = 0;
	data = *data_p;

	if(!data)
	{
		endofscript = true;
		*data_p = NULL;
		return false;
	}

// skip whitespace
skipwhite:
	for (;*data <= ' ';data++)
	{
		if (*data == 0)
		{
			// end of file
			endofscript = true;
			*data_p = NULL;
			return false;
		}
	}

	if (*data == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data && *data != '\"';data++)
		{
			// allow escaped " and \ case
			if (*data == '\\' && (data[1] == '\"' || data[1] == '\\'))
				data++;
			if (len < (int)sizeof(token) - 1)
				token[len++] = *data;
		}
		token[len] = 0;
		if (*data == '\"') data++;
		*data_p = data;
	}
	else
	{
		// regular word
		for (;*data > ' ';data++)
			if (len < (int)sizeof(token) - 1)
				token[len++] = *data;
		token[len] = 0;
		*data_p = data;
	}
	return true;
}

/*
==============
SC_ParseToken

Parse a token out of a string
==============
*/
char *SC_ParseToken( const char **data_p, bool allow_newline )
{
	int		c;
	int		len = 0;
	const char	*data;
	bool		newline = false;
		
	token[0] = 0;
	data = *data_p;
	
	if( !data ) 
	{
		endofscript = true;
		*data_p = NULL;
		return token;
	}		

	while( 1 )
	{
		data = SC_SkipWhiteSpace( data, &newline );
		if( !data )
		{
			*data_p = NULL;
			return token;
		}
		if( newline && !allow_newline )
		{
			*data_p = data;
			return token;
		}
		
		c = *data;
	
		if( c=='/' && data[1] == '/' )
		{
			// skip // comments
			while( *data && *data != '\n' )
				data++;
		}
		else if( c=='/' && data[1] == '*' )
		{
			// skip /* comments
			while( data[1] && (data[0] != '*' || data[1] != '/'))
			{
				if( *data == '\n' ) com.com_scriptline++;
				data++;
			}
			if( *data ) data += 2;
		}
		else break; // an actual token
	}	

	// handle quoted strings specially
	if (*data == '\"' || *data == '\'')
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c == '\n' ) com.com_scriptline++;
			if( c=='\"' || c=='\0' )
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len++] = c;
		}
	}

	// parse single characters
	if( c == '{' || c == '}'|| c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		token[len] = c;
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;
	*data_p = data;
	return token;
}

/*
==============
SC_ParseWord

Parse a word out of a string
==============
*/
char *SC_ParseWord( const char **data_p, bool allow_newline )
{
	int		c;
	const char	*data;
	int		len = 0;
	bool		newline = false;
	
	token[0] = 0;
	data = *data_p;
	
	if( !data )
	{
		*data_p = NULL;
		return NULL;
	}
		
	while( 1 )
	{
		data = SC_SkipWhiteSpace( data, &newline );
		if( !data )
		{
			*data_p = NULL;
			return NULL;
		}
		if( newline && !allow_newline )
		{
			*data_p = data;
			return token;
		}
		
		c = *data;
	
		if( c=='/' && data[1] == '/' )
		{
			// skip // comments
			while( *data && *data != '\n' )
				data++;
		}
		else if( c=='/' && data[1] == '*' )
		{
			// skip /* comments
			while( data[1] && (data[0] != '*' || data[1] != '/'))
			{
				if( *data == '\n' ) com.com_scriptline++;
				data++;
			}
			if( *data ) data += 2;
		}
		else break; // an actual token
	}	

	// handle quoted strings specially
	if( c == '\"' )
	{
		data++;
		do
		{
			c = *data++;
			if (c=='\"' || c=='\0')
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		} while( 1 );
	}

	// parse numbers
	if( c >= '0' && c <= '9' )
	{
		if( c == '0' && data[1] == 'x' )
		{
			// parse hex
			token[0] = '0';
			c='x';
			len=1;
			data++;
			while( 1 )
			{
				// parse regular number
				token[len] = c;
				data++;
				len++;
				c = *data;
				if ((c < '0'|| c > '9') && (c < 'a'||c > 'f') && (c < 'A'|| c > 'F') && c != '.')
					break;
			}

		}
		else
		{
			while( 1 )
			{
				// parse regular number
				token[len] = c;
				data++;
				len++;
				c = *data;
				if ((c < '0'|| c > '9') && c != '.')
					break;
			}
		}
		
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse words
	else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
	{
		do
		{
			token[len] = c;
			data++;
			len++;
			c = *data;
		} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
		
		token[len] = 0;
		*data_p = data;
		return token;
	}
	else
	{
		token[len] = c;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}
}
/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/

/*
==============
Match Token With

check current token for match with user keyword
==============
*/
bool SC_MatchToken( const char *match )
{
	if (!com_stricmp( token, match ))
		return true;
	return false;
}

/*
==============
SC_SkipToken

skip current token and jump into newline
==============
*/
void SC_SkipToken( void )
{
	switch( Sys.app_name )
	{
	case HOST_BSPLIB:
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		// don't handle single characters
		SC_ReadTokenSimple( false );
		break;
	default:
		SC_ReadToken( false );
		break;
	}
	tokenready = true;
}

/*
==============
SC_FreeToken

release current token to get 
him again with SC_GetToken()
==============
*/
void SC_FreeToken( void )
{
	tokenready = true;
}

/*
==============
SC_TryToken

check token for available on current line
==============
*/
bool SC_TryToken( void )
{
	if(!SC_TokenAvailable())
		return false;

	switch( Sys.app_name )
	{
	case HOST_BSPLIB:
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		// don't handle single characters
		SC_ReadTokenSimple( false );
		break;
	default:
		SC_ReadToken( false );
		break;
	}
	return true;
}


/*
==============
SC_GetToken

get token on current or newline
==============
*/
char *SC_GetToken( bool newline )
{
	switch( Sys.app_name )
	{
	case HOST_BSPLIB:
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		// don't handle single characters
		if(SC_ReadTokenSimple( newline ))
			return token;
		break;
	default:
		if(SC_ReadToken( newline ))
			return token;
		break;
	}
	return NULL;
}

/*
==============
SC_Token

return current token
==============
*/
char *SC_Token( void )
{
	return token;
}

/*
============
SC_StringContains
============
*/
char *SC_StringContains(char *str1, char *str2, int casecmp)
{
	int	len, i, j;

	len = com_strlen(str1) - com_strlen(str2);

	for (i = 0; i <= len; i++, str1++)
	{
		for (j = 0; str2[j]; j++)
		{
			if (casecmp)
			{
				if (str1[j] != str2[j]) break;
			}
			else
			{
				if(com_toupper(str1[j]) != com_toupper(str2[j]))
					break;
			}
		}
		if (!str2[j]) return str1;
	}
	return NULL;
}

/*
============
SC_FilterToken
============
*/
bool SC_FilterToken(char *filter, char *name, int casecmp)
{
	char	buf[MAX_MSGLEN];
	char	*ptr;
	int	i, found;

	while(*filter)
	{
		if(*filter == '*')
		{
			filter++;
			for (i = 0; *filter; i++)
			{
				if (*filter == '*' || *filter == '?')
					break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (com_strlen(buf))
			{
				ptr = SC_StringContains(name, buf, casecmp);
				if (!ptr) return false;
				name = ptr + com_strlen(buf);
			}
		}
		else if (*filter == '?')
		{
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[')
		{
			filter++;
		}
		else if (*filter == '[')
		{
			filter++;
			found = false;
			while(*filter && !found)
			{
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']'))
				{
					if (casecmp)
					{
						if (*name >= *filter && *name <= *(filter+2)) found = true;
					}
					else
					{
						if (com_toupper(*name) >= com_toupper(*filter) && com_toupper(*name) <= com_toupper(*(filter+2)))
							found = true;
					}
					filter += 3;
				}
				else
				{
					if (casecmp)
					{
						if (*filter == *name) found = true;
					}
					else
					{
						if (com_toupper(*filter) == com_toupper(*name)) found = true;
					}
					filter++;
				}
			}
			if (!found) return false;
			while(*filter)
			{
				if (*filter == ']' && *(filter+1) != ']')
					break;
				filter++;
			}
			filter++;
			name++;
		}
		else
		{
			if (casecmp)
			{
				if (*filter != *name)
					return false;
			}
			else
			{
				if (com_toupper(*filter) != com_toupper(*name))
					return false;
			}
			filter++;
			name++;
		}
	}
	return true;
}

/*
=================
SC_HashKey

returns hash key for string
=================
*/
uint SC_HashKey( const char *string, uint hashSize )
{
	uint	hashKey = 0;
	int	i;

	for( i = 0; string[i]; i++ )
		hashKey = (hashKey + i) * 37 + com_tolower(string[i]);

	return (hashKey % hashSize);
}