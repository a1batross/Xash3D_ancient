//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         parselib.c - script files parser
//=======================================================================

#include "launch.h"

typedef struct
{
	char	filename[1024];
	byte	*buffer; 
	byte	*script_p;
	byte	*end_p;
	int	line;
} script_t;

// max included scripts
#define	MAX_INCLUDES	32

script_t	scriptstack[ MAX_INCLUDES ];
script_t	*script;
int	scriptline;

char token[MAX_MSGLEN]; // contains token info
static const char *saved_token;
static int saved_scriptline;

bool endofscript;
bool tokenready; // only true if UnGetToken was just called
bool SC_EndOfScript (bool newline);

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
	script->line = scriptline = 1;
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
	script->line = scriptline = saved_scriptline = 1;
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
	saved_scriptline = scriptline;
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
	scriptline = saved_scriptline;
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
			scriptline = script->line++;
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
		scriptline = script->line++;
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
				scriptline = script->line++;
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
				MsgDev(D_WARN, "GetToken: Token too large on line %i\n", scriptline);
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
				MsgDev( D_WARN, "GetToken: Token too large on line %i\n",scriptline);
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

	if (tokenready) // is a token allready waiting?
	{
		tokenready = false;
		return true;
	}

	if (script->script_p >= script->end_p)
		return SC_EndOfScript( newline );

	
skip_whitespace:	// skip whitespace
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return SC_EndOfScript (newline);

		if (*script->script_p++ == '\n')
		{
			if (!newline) goto line_incomplete;
			scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p)
		return SC_EndOfScript (newline);

	// ; # // comments
	if (*script->script_p == ';' || *script->script_p == '#' || ( script->script_p[0] == '/' && script->script_p[1] == '/') )
	{
		if (!newline) goto line_incomplete;

		// ets+++
		if(*script->script_p == '/') script->script_p++;
		if(script->script_p[1] == 'T' && script->script_p[2] == 'X')
		{
			GI.TXcommand = script->script_p[3];//TX#"-style comment
			Msg("Quark TX command %s\n", GI.TXcommand );
		}
		while (*script->script_p++ != '\n')
		{
			if (script->script_p >= script->end_p)
				return SC_EndOfScript (newline);
		}
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
				scriptline = script->line++;
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
				MsgDev(D_WARN, "GetToken: Token too large on line %i\n", scriptline);
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
				MsgDev( D_WARN, "GetToken: Token too large on line %i\n",scriptline);
				break;
			}
		
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
          }
          
	*token_p = 0;

	//quake style include & default MSVC style
	if (!strcmp(token, "$include"))
	{
		SC_ReadTokenSimple(false);
		SC_AddScript(token, NULL, 0 );
		return SC_ReadTokenSimple(newline);
	}
	return true;

line_incomplete:
	//invoke error
	return SC_EndOfScript( newline );
}
 
/*
==============
SC_EndOfScript
==============
*/
bool SC_EndOfScript( bool newline )
{
	if(!newline) 
	{
		scriptline = script->line;
		Sys_Break("%s: line %i is incomplete\n", script->filename, scriptline);
	}

	if(!com_strcmp(script->filename, "*sc_buffer"))
	{
		endofscript = true;
		return false;
	}

	// FIXME: stupid xash bug
	if(Mem_IsAllocated( script->buffer ))
		Mem_Free( script->buffer );
	if(script == scriptstack + 1)
	{
		endofscript = true;
		return false;
	}

	script--;
	token[0] = 0; // clear last token
	scriptline = script->line;
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
			scriptline++;
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
				if( *data == '\n' ) scriptline++;
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
			if( c == '\n' ) scriptline++;
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
				if( *data == '\n' ) scriptline++;
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
	SC_ReadToken( true );
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

	SC_ReadToken( false );
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