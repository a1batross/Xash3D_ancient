//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         parselib.h - parser simple types
//=======================================================================
#ifndef PARSELIB_H
#define PARSELIB_H

#define MAX_TOKEN_LENGTH	MAX_SYSPATH

// number sub-types
enum
{
	NT_BINARY		= BIT(0),
	NT_OCTAL		= BIT(1),
	NT_DECIMAL	= BIT(2),
	NT_HEXADECIMAL	= BIT(3),
	NT_INTEGER	= BIT(4),
	NT_FLOAT		= BIT(5),
	NT_UNSIGNED	= BIT(6),
	NT_LONG		= BIT(7),
	NT_SINGLE		= BIT(8),
	NT_DOUBLE		= BIT(9),
	NT_EXTENDED	= BIT(10),
};

// punctuation sub-types
enum
{
	PT_RSHIFT_ASSIGN = 1,
	PT_LSHIFT_ASSIGN,
	PT_PARAMETERS,
	PT_PRECOMPILER_MERGE,
	PT_LOGIC_AND,
	PT_LOGIC_OR,
	PT_LOGIC_GEQUAL,
	PT_LOGIC_LEQUAL,
	PT_LOGIC_EQUAL,
	PT_LOGIC_NOTEQUAL,
	PT_MUL_ASSIGN,
	PT_DIV_ASSIGN,
	PT_MOD_ASSIGN,
	PT_ADD_ASSIGN,
	PT_SUB_ASSIGN,
	PT_INCREMENT,
	PT_DECREMENT,
	PT_BINARY_AND_ASSIGN,
	PT_BINARY_OR_ASSIGN,
	PT_BINARY_XOR_ASSIGN,
	PT_RSHIFT,
	PT_LSHIFT,
	PT_POINTER_REFERENCE,
	PT_CPP_1,
	PT_CPP_2,
	PT_MUL,
	PT_DIV,
	PT_MOD,
	PT_ADD,
	PT_SUB,
	PT_ASSIGN,
	PT_BINARY_AND,
	PT_BINARY_OR,
	PT_BINARY_XOR,
	PT_BINARY_NOT,
	PT_LOGIC_NOT,
	PT_LOGIC_GREATER,
	PT_LOGIC_LESS,
	PT_REFERENCE,
	PT_COLON,
	PT_COMMA,
	PT_SEMICOLON,
	PT_QUESTION_MARK,
	PT_BRACE_OPEN,
	PT_BRACE_CLOSE,
	PT_BRACKET_OPEN,
	PT_BRACKET_CLOSE,
	PT_PARENTHESIS_OPEN,
	PT_PARENTHESIS_CLOSE,
	PT_PRECOMPILER,
	PT_DOLLAR,
	PT_BACKSLASH,
	PT_MAXCOUNT,
};

typedef struct
{
	const char	*name;
	uint		type;
} punctuation_t;

typedef struct script_s
{
	// shared part of script
	char		*buffer;
	char		*text;
	size_t		size;
	char		TXcommand;	// (quark .map comment)

	// private part of script
	string		name;
	int		line;
	bool		allocated;
	punctuation_t	*punctuations;
	bool		tokenAvailable;
	token_t		token;
};

bool PS_ReadToken( script_t *script, scFlags_t flags, token_t *token );
void PS_SaveToken( script_t *script, token_t *token );
bool PS_GetString( script_t *script, int flags, char *value, size_t size );
bool PS_GetDouble( script_t *script, int flags, double *value );
bool PS_GetFloat( script_t *script, int flags, float *value );
bool PS_GetUnsigned( script_t *script, int flags, uint *value );
bool PS_GetInteger( script_t *script, int flags, int *value );

void PS_SkipWhiteSpace( script_t *script );
void PS_SkipRestOfLine( script_t *script );
void PS_SkipBracedSection( script_t *script, int depth );

void PS_ScriptError( script_t *script, scFlags_t flags, const char *fmt, ... );
void PS_ScriptWarning( script_t *script, scFlags_t flags, const char *fmt, ... );

bool PS_MatchToken( token_t *token, const char *keyword );
void PS_SetPunctuationsTable( script_t *script, punctuation_t *punctuationsTable );
void PS_ResetScript( script_t *script );
bool PS_EndOfScript( script_t *script );

script_t	*PS_LoadScript( const char *filename, const char *buf, size_t size );
void	PS_FreeScript( script_t *script );


#endif//PARSELIB_H