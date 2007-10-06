//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pr_lex.c - qcclib lexemes set
//=======================================================================

#include "qcclib.h"
#include "time.h"

char	*compilingfile;
int	pr_source_line;
char	*pr_file_p;
char	*pr_line_start; // start of current source line
int	pr_bracelevel;
char	pr_token[8192];
token_type_t pr_token_type;
type_t	*pr_immediate_type;
eval_t	pr_immediate;
char	pr_immediate_string[8192];
int	pr_error_count;
int	pr_warning_count;
int	pr_total_error_count;
const_t	*CompilerConstant;
int	numCompilerConstants;
void	*errorscope;
int	ForcedCRC;
bool	recursivefunctiontype;

// read on until the end of the line
#define GoToEndLine() while(*pr_file_p != '\n' && *pr_file_p != '\0'){ pr_file_p++; }

// longer symbols must be before a shorter partial match
char *pr_punctuation1[] = {"&&", "||", "<=", ">=","==", "!=", "/=", "*=", "+=", "-=", "(+)", "|=", "(-)", "++", "--", "->", "::", ";", ",", "!", "*", "/", "(", ")", "-", "+", "=", "[", "]", "{", "}", "...", "..", ".", "<<", "<", ">>", ">" , "#" , "@", "&" , "|", "^", ":", NULL};
char *pr_punctuation2[] = {"&&", "||", "<=", ">=","==", "!=", "/=", "*=", "+=", "-=", "|=",  "|=", "(-)", "++", "--", ".",  "::", ";", ",", "!", "*", "/", "(", ")", "-", "+", "=", "[", "]", "{", "}", "...", "..", ".", "<<", "<", ">>", ">" , "#" , "@", "&" , "|", "^", ":", NULL};

// simple types.  function types are dynamically allocated
type_t	*type_void;
type_t	*type_string;
type_t	*type_float;
type_t	*type_vector;
type_t	*type_entity;
type_t	*type_field;
type_t	*type_function;
type_t	*type_pointer;
type_t	*type_integer;
type_t	*type_variant;
type_t	*type_floatfield;

const int	type_size[12] = 
{
		1,	// void
	sizeof(string_t)/4,	// string
		1,	// float
		3,	// vector
		1,	// entity
		1,	// field
	sizeof(func_t)/4,	// function
	sizeof(void *)/4,	// pointer
		1,	// integer
		1,	// FIXME: how big should a variant be?
		0,	// ev_struct. variable sized.
		0	// ev_union. variable sized.
};

char pr_parm_names[MAX_PARMS + MAX_PARMS_EXTRA][MAX_NAME];
def_t def_ret, def_parms[MAX_PARMS];
includechunk_t *currentchunk;
char pevname[8];	// Xash Progs have keyword "pev" not "self"
char opevname[8];	// class self name (opev or oself)

void PR_IncludeChunk (char *data, bool duplicate, char *filename)
{
	includechunk_t *chunk = Qalloc(sizeof(includechunk_t));
	chunk->prev = currentchunk;
	currentchunk = chunk;

	chunk->currentdatapoint = pr_file_p;
	chunk->currentlinenumber = pr_source_line;

	if (duplicate)
	{
		pr_file_p = Qalloc(strlen(data)+1);
		strcpy(pr_file_p, data);
	}
	else pr_file_p = data;
}

bool PR_UnInclude(void)
{
	if (!currentchunk) return false;
	pr_file_p = currentchunk->currentdatapoint;
	pr_source_line = currentchunk->currentlinenumber;

	currentchunk = currentchunk->prev;
	return true;
}

type_t *PR_NewType (char *name, int basictype)
{
	if (numtypeinfos>= maxtypeinfos) PR_ParseError(ERR_INTERNAL, "Too many types");
	memset(&qcc_typeinfo[numtypeinfos], 0, sizeof(type_t));
	qcc_typeinfo[numtypeinfos].type = basictype;
	qcc_typeinfo[numtypeinfos].name = name;
	qcc_typeinfo[numtypeinfos].num_parms = 0;
	qcc_typeinfo[numtypeinfos].param = NULL;
	qcc_typeinfo[numtypeinfos].size = type_size[basictype];	

	numtypeinfos++;
	return &qcc_typeinfo[numtypeinfos-1];
}

void PR_FindBestInclude(char *newfile, char *currentfile, char *rootpath)
{
	char	fullname[10248];
	char	*stripfrom;
	int	doubledots;
	char	*end = fullname;

	if (!*newfile) return;

	doubledots = 0;
	while(!strncmp(newfile, "../", 3) || !strncmp(newfile, "..\\", 3))
	{
		newfile += 3;
		doubledots++;
	}

	currentfile += strlen(rootpath); // could this be bad?

	for(stripfrom = currentfile + strlen(currentfile) - 1; stripfrom>currentfile; stripfrom--)
	{
		if (*stripfrom == '/' || *stripfrom == '\\')
		{
			if (doubledots>0) doubledots--;
			else
			{
				stripfrom++;
				break;
			}
		}
	}

	strcpy(end, rootpath); 
	end = end + strlen(end);

	if (*fullname && end[-1] != '/')
	{
		strcpy(end, "/");
		end = end+strlen(end);
	}

	strncpy(end, currentfile, stripfrom - currentfile); 
	end += stripfrom - currentfile; *end = '\0';
	strcpy(end, newfile);

	PR_Include(fullname);
}

/*
==============
PR_Precompiler

Runs precompiler stage
==============
*/
bool PR_Precompiler(void)
{
	char		buf[1024];
	char		*msg = buf;
	int		ifmode;
	int		a;
	static int	ifs = 0;
	int		level;	//#if level
	bool		eval = false;

	if (*pr_file_p == '#')
	{
		char *directive;
		for (directive = pr_file_p + 1; *directive; directive++) // so #    define works
		{
			if (*directive == '\r' || *directive == '\n')
				PR_ParseError(ERR_UNKNOWNPUCTUATION, "Hanging # with no directive\n");
			if (*directive > ' ') break;
		}
		if (!strncmp(directive, "define", 6))
		{
			pr_file_p = directive;
			PR_ConditionCompilation();
			GoToEndLine();
		}
		else if (!strncmp(directive, "undef", 5))
		{
			pr_file_p = directive+5;
			while(*pr_file_p <= ' ') pr_file_p++;

			PR_SimpleGetToken ();
			PR_UndefineName(pr_token);
                              GoToEndLine();
		}
		else if (!strncmp(directive, "if", 2))
		{
			int originalline = pr_source_line;
 			pr_file_p = directive + 2;
			if (!strncmp(pr_file_p, "def ", 4))
			{
				ifmode = 0;
				pr_file_p+=4;
			}
			else if (!strncmp(pr_file_p, "ndef ", 5))
			{
				ifmode = 1;
				pr_file_p+=5;
			}
			else
			{
				ifmode = 2;
				pr_file_p+=0;
			}

			PR_SimpleGetToken ();
			level = 1;

                              GoToEndLine();

			if (ifmode == 2)
			{
				if (atof(pr_token)) eval = true;
			}
			else
			{
				if (PR_CheckCompConstDefined(pr_token))
					eval = true;

				if (ifmode == 1) eval = eval ? false : true; //same as eval = !eval		
			}

			if (eval) ifs++;
			else
			{
				while (1)
				{
					while(*pr_file_p && (*pr_file_p==' ' || *pr_file_p == '\t'))
						pr_file_p++;

					if (!*pr_file_p)
					{
						pr_source_line = originalline;
						PR_ParseError (ERR_NOENDIF, "#if with no endif");
					}

					if (*pr_file_p == '#')
					{
						pr_file_p++;
						while(*pr_file_p==' ' || *pr_file_p == '\t')
							pr_file_p++;
						if (!strncmp(pr_file_p, "endif", 5))
							level--;
						if (!strncmp(pr_file_p, "if", 2))
							level++;
						if (!strncmp(pr_file_p, "else", 4) && level == 1)
						{
							ifs++;
							GoToEndLine();
						}
					}

                              		GoToEndLine();
					if (level <= 0) break;
					pr_file_p++; // next line
					pr_source_line++;
				}
			}
		}
		else if (!strncmp(directive, "else", 4))
		{
			int originalline = pr_source_line;

			ifs -= 1;
			level = 1;

                              GoToEndLine();
			while (1)
			{
				while(*pr_file_p && (*pr_file_p==' ' || *pr_file_p == '\t'))
					pr_file_p++;

				if (!*pr_file_p)
				{
					pr_source_line = originalline;
					PR_ParseError(ERR_NOENDIF, "#if with no endif");
				}

				if (*pr_file_p == '#')
				{
					pr_file_p++;
					while(*pr_file_p==' ' || *pr_file_p == '\t')
						pr_file_p++;

					if (!strncmp(pr_file_p, "endif", 5))
						level--;
					if (!strncmp(pr_file_p, "if", 2))
							level++;
					if (!strncmp(pr_file_p, "else", 4) && level == 1)
					{
						ifs+=1;
						break;
					}
				}

                              	GoToEndLine();
				if (level <= 0) break;
				pr_file_p++; // go off the end
				pr_source_line++;
			}
		}
		else if (!strncmp(directive, "endif", 5))
		{		
                              GoToEndLine();
			if (ifs <= 0) PR_ParseError(ERR_NOPRECOMPILERIF, "unmatched #endif");
			else ifs -= 1;
		}
		else if (!strncmp(directive, "eof", 3))
		{
			pr_file_p = NULL;
			return true;
		}
		else if (!strncmp(directive, "error", 5))
		{		
			pr_file_p = directive+5;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			GoToEndLine();
			PR_ParseError(ERR_PRECOMPILERMESSAGE, "#error: %s", msg);
		}
		else if (!strncmp(directive, "warning", 7))
		{		
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			GoToEndLine();
			PR_ParseWarning(WARN_PRECOMPILERMESSAGE, "#warning: %s", msg);
		}
		else if (!strncmp(directive, "message", 7))
		{		
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			GoToEndLine();
			PR_Message("#message: %s\n", msg);
		}
		else if (!strncmp(directive, "copyright", 9))
		{
			pr_file_p = directive+9;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			GoToEndLine();
			if (strlen(msg) >= sizeof(v_copyright))
				PR_ParseWarning(WARN_STRINGTOOLONG, "Copyright message is too long\n");
			strncpy(v_copyright, msg, sizeof(v_copyright)-1);
		}
		else if (!strncmp(directive, "forcecrc", 8))
		{		
			pr_file_p=directive+8;

			ForcedCRC = PR_LexInteger();					

			pr_file_p++;
				
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';
			GoToEndLine();	
		}
		else if (!strncmp(directive, "includelist", 11))
		{
			pr_file_p=directive+11;

			while(*pr_file_p <= ' ') pr_file_p++;

			while(1)
			{
				PR_LexWhitespace();
				if (!PR_SimpleGetToken())
				{
					if (!*pr_file_p)
						PR_ParseError(ERR_INTERNAL, "eof in includelist");
					else
					{
						pr_file_p++;
						pr_source_line++;
					}
					continue;
				}
				if (!strcmp(pr_token, "#endlist"))
					break;

				PR_FindBestInclude(pr_token, compilingfile, qccmsourcedir);

				if (*pr_file_p == '\r')
					pr_file_p++;

				for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
					msg[a] = pr_file_p[a];

				msg[a-1] = '\0';

				GoToEndLine();
			}
                              GoToEndLine();
		}
		else if (!strncmp(directive, "include", 7))
		{
			char sm;

			pr_file_p=directive+7;

			while(*pr_file_p <= ' ') pr_file_p++;

			msg[0] = '\0';
			if (*pr_file_p == '\"') sm = '\"';
			else if (*pr_file_p == '<') sm = '>';
			else
			{
				PR_ParseError(0, "Not a string literal (on a #include)");
				sm = 0;
			}

			pr_file_p++;
			a = 0;

			while(*pr_file_p != sm)
			{
				if (*pr_file_p == '\n')
				{
					PR_ParseError(0, "#include continued over line boundy\n");
					break;
				}
				msg[a++] = *pr_file_p;
				pr_file_p++;
			}
			msg[a] = 0;

			PR_FindBestInclude(msg, compilingfile, qccmsourcedir);
			pr_file_p++;

			while(*pr_file_p != '\n' && *pr_file_p != '\0' && *pr_file_p <= ' ')
				pr_file_p++;
			GoToEndLine();
		}
		else if (!strncmp(directive, "library", 8))
		{		
			pr_file_p=directive+8;

			while(*pr_file_p <= ' ') pr_file_p++;

			PR_LexString();
			PR_Message("Including library: %s\n", pr_token);
			QCC_LoadData(pr_token);

			pr_file_p++;

			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';
                              GoToEndLine();
		}
		else if (!strncmp(directive, "output", 6))
		{
			pr_file_p = directive + 6;

			while(*pr_file_p <= ' ') pr_file_p++;

			PR_LexString();
			strcpy(destfile, pr_token);
			PR_Message("Outputfile: %s\n", destfile);

			pr_file_p++;
				
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';
			GoToEndLine();
		}
		else if (!strncmp(directive, "pragma", 6))
		{
			pr_file_p = directive+6;
			while(*pr_file_p <= ' ') pr_file_p++;

			token[0] = '\0';
			for(a = 0; *pr_file_p != '\n' && *pr_file_p != '\0'; pr_file_p++) // read on until the end of the line
			{
				if ((*pr_file_p == ' ' || *pr_file_p == '\t'|| *pr_file_p == '(') && !*token)
				{
					msg[a] = '\0';
					strcpy(token, msg);
					a=0;
					continue;
				}
				msg[a++] = *pr_file_p;
			}
			
			msg[a] = '\0';
			{
				char *end;
				for (end = msg + a-1; end>=msg && *end <= ' '; end--)
					*end = '\0';
			}

			if (!*token)
			{
				strcpy(token, msg);
				msg[0] = '\0';
			}

			{
				char *end;
				for (end = msg + a-1; end>=msg && *end <= ' '; end--)
					*end = '\0';
			}

			if (!stricmp(token, "DONT_COMPILE_THIS_FILE"))
			{
				while (*pr_file_p)
				{
					GoToEndLine();
					if (*pr_file_p == '\n')
					{
						PR_NewLine(false);
						pr_file_p++;
					}
				}
			}
			else if (!stricmp(token, "COPYRIGHT"))
			{
				if (strlen(msg) >= sizeof(v_copyright))
					PR_ParseWarning(WARN_STRINGTOOLONG, "Copyright message is too long\n");
				strncpy(v_copyright, msg, sizeof(v_copyright)-1);
			}
			else if (!strncmp(directive, "forcecrc", 8))
			{
				ForcedCRC = atoi(msg);
			}
			else if (!stricmp(token, "TARGET"))
			{
				if (!stricmp(msg, "STANDARD")) target_version = QPROGS_VERSION;
				else if (!stricmp(msg, "ID")) target_version = QPROGS_VERSION;
				else if (!stricmp(msg, "FTE")) target_version = FPROGS_VERSION;
				else if (!stricmp(msg, "VPROGS"))target_version = VPROGS_VERSION;
				else PR_ParseWarning(WARN_BADTARGET, "Unknown target \'%s\'. Ignored.", msg);
			}
			else if (!stricmp(token, "keyword") || !stricmp(token, "flag"))
			{
				int	st;

				SC_ParseToken(&msg);

				if (!stricmp(token, "enable") || !stricmp(token, "on")) st = 1;
				else if (!stricmp(token, "disable") || !stricmp(token, "off")) st = 0;
				else
				{
					PR_ParseWarning(WARN_BADPRAGMA, "compiler flag state not recognised");
					st = -1;
				}
				if (st < 0) PR_ParseWarning(WARN_BADPRAGMA, "warning id not recognized");
				else
				{
					int f;
					SC_ParseToken(&msg);

					for (f = 0; compiler_flag[f].enabled; f++)
					{
						if (!stricmp(compiler_flag[f].name, token))
						{
							if (compiler_flag[f].flags & FLAG_MIDCOMPILE)
								*compiler_flag[f].enabled = st;
							else PR_ParseWarning(WARN_BADPRAGMA, "Cannot enable/disable keyword/flag via a pragma");
							break;
						}
					}
					if (!compiler_flag[f].enabled)
						PR_ParseWarning(WARN_BADPRAGMA, "keyword/flag not recognised");

				}
			}
			else if (!stricmp(token, "warning"))
			{
				int st;

				SC_ParseToken(&msg);
				if (!stricmp(token, "enable") || !stricmp(token, "on")) st = 0;
				else if (!stricmp(token, "disable") || !stricmp(token, "off")) st = 1;
				else if (!stricmp(token, "toggle")) st = 2;
				else
				{
					PR_ParseWarning(WARN_BADPRAGMA, "warning state not recognized");
					st = -1;
				}
				if (st >= 0)
				{
					int wn;
					SC_ParseToken(&msg); // just a number of warning
					wn = atoi(token);
					if (wn < 0 || wn > WARN_CONSTANTCOMPARISON)
					{
						PR_ParseWarning(WARN_BADPRAGMA, "warning id not recognised");
					}
					else
					{
						if (st == 2) pr_warning[wn] = true - pr_warning[wn];
						else pr_warning[wn] = st;
					}
				}
			}
			else PR_ParseWarning(WARN_BADPRAGMA, "Unknown pragma \'%s\'", token);
		}
		return true;
	}
	return false;
}

/*
==============
PR_NewLine

Call at start of file and when *pr_file_p == '\n'
==============
*/
void PR_NewLine (bool incomment)
{
	bool	m;
	
	if (*pr_file_p == '\n')
	{
		pr_file_p++;
		m = true;
	}
	else m = false;

	pr_source_line++;
	pr_line_start = pr_file_p;
	while(*pr_file_p==' ' || *pr_file_p == '\t') pr_file_p++;

	if (incomment); // no constants if in a comment.
	else if (PR_Precompiler());
	if (m) pr_file_p--;
}

/*
==============
PR_LexString

Parses a quoted string
==============
*/
void PR_LexString (void)
{
	int		c;
	int		len = 0;
	char		*end, *cnst;
	int 		texttype = 0;
	
	pr_file_p++;

	do
	{
		c = *pr_file_p++;
		if (!c) PR_ParseError (ERR_EOF, "EOF inside quote");
		if (c=='\n') PR_ParseError (ERR_INVALIDSTRINGIMMEDIATE, "newline inside quote");
		if (c=='\\')
		{	
			// escape char
			c = *pr_file_p++;
			if (!c) PR_ParseError (ERR_EOF, "EOF inside quote");
			if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == '"') c = '"';
			else if (c == 't') c = '\t';
			else if (c == 'a') c = '\a';
			else if (c == 'v') c = '\v';
			else if (c == 'f') c = '\f';
			else if (c == 's' || c == 'b')
			{
				texttype ^= 128;
				continue;
			}
			else if (c == '[') c = 16;
			else if (c == ']') c = 17;
			else if (c == '{')
			{
				int d;
				c = 0;
				while ((d = *pr_file_p++) != '}')
				{
					c = c * 10 + d - '0';
					if (d < '0' || d > '9' || c > 255)
						PR_ParseError(ERR_BADCHARACTURECODE, "Bad character code");
				}
			}
			else if (c == '<') c = 29;
			else if (c == '-') c = 30;
			else if (c == '>') c = 31;
			else if (c == '\\') c = '\\';
			else if (c == '\'') c = '\'';
			else if (c >= '0' && c <= '9') c = 18 + c - '0';
			else if (c == '\r')
			{	
				c = *pr_file_p++;// sigh
				if (c != '\n') PR_ParseWarning(WARN_HANGINGSLASHR, "Hanging \\\\\r");
				pr_source_line++;
			}
			else if (c == '\n')
			{	
				pr_source_line++;// sigh
			}
			else PR_ParseError (ERR_INVALIDSTRINGIMMEDIATE, "Unknown escape char %c", c);
		}
		else if (c=='\"')
		{
			if (len >= sizeof(pr_immediate_string) - 1)
				PR_ParseError(ERR_INTERNAL, "String length exceeds %i", sizeof(pr_immediate_string)-1);

			while(*pr_file_p && *pr_file_p <= ' ')
			{
				if (*pr_file_p == '\n')
					PR_NewLine(false);
				pr_file_p++;
			}
			if (*pr_file_p == '\"')
			{
				// have annother go
				pr_file_p++;
				continue;
			}
			pr_token[len] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = type_string;
			strcpy (pr_immediate_string, pr_token);			
			return;
		}
		else if (c == '#')
		{
			for (end = pr_file_p;; end++)
			{
				if (*end <= ' ') break;
				if (*end == ')') break;
				if (*end == '(') break;
				if (*end == '+') break;
				if (*end == '-') break;
				if (*end == '*') break;
				if (*end == '/') break;
				if (*end =='\\') break;
				if (*end == '|') break;
				if (*end == '&') break;
				if (*end == '=') break;
				if (*end == '^') break;
				if (*end == '~') break;
				if (*end == '[') break;
				if (*end == ']') break;
				if (*end =='\"') break;
				if (*end == '{') break;
				if (*end == '}') break;
				if (*end == ';') break;
				if (*end == ':') break;
				if (*end == ',') break;
				if (*end == '.') break;
				if (*end == '#') break;
			}

			c = *end;
			*end = '\0';
			cnst = PR_CheakCompConstString(pr_file_p);
			if (cnst == pr_file_p) cnst = NULL;
			*end = c;
			c = '#';	// undo
			if (cnst)
			{
				PR_ParseWarning(WARN_MACROINSTRING, "Macro expansion in string");
				if (len+strlen(cnst) >= sizeof(pr_token)-1)
					PR_ParseError(ERR_INTERNAL, "String length exceeds %i", sizeof(pr_token)-1);
				strcpy(pr_token+len, cnst);
				len+=strlen(cnst);
				pr_file_p = end;
				continue;
			}
		}
		else c |= texttype;

		pr_token[len] = c;
		len++;
		if (len >= sizeof(pr_token)-1)
			PR_ParseError(ERR_INTERNAL, "String length exceeds %i", sizeof(pr_token)-1);
	} while (1);
}

/*
==============
PR_LexNumber
==============
*/
int PR_LexInteger (void)
{
	int	c = *pr_file_p;
	int	len = 0;
	
	if (pr_file_p[0] == '0' && pr_file_p[1] == 'x')
	{
		pr_token[0] = '0';
		pr_token[1] = 'x';
		len = 2;
		c = *(pr_file_p+=2);
	}
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ((c >= '0' && c<= '9') || c == '.' || (c>='a' && c <= 'f'));

	pr_token[len] = 0;
	return atoi (pr_token);
}

void PR_LexNumber (void)
{
	int	num = 0;
	int	base = 10;
	int	c;
	int	sign = 1;

	if (*pr_file_p == '-')
	{
		sign = -1;
		pr_file_p++;
	}
	if (pr_file_p[1] == 'x')
	{
		pr_file_p += 2;
		base = 16;
	}

	while((c = *pr_file_p))
	{		
		if (c >= '0' && c <= '9')
		{
			num*=base;
			num += c-'0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			num*=base;
			num += c -'a'+10;
		}
		else if (c >= 'A' && c <= 'F')
		{
			num*=base;
			num += c -'A'+10;
		}
		else if (c == '.')
		{
			pr_file_p++;
			pr_immediate_type = type_float;
			pr_immediate._float = (float)num;
			num = 1;
			while(1)
			{
				c = *pr_file_p;
				if (c >= '0' && c <= '9')
				{
					num*=base;
					pr_immediate._float += (c-'0')/(float)(num);
				}
				else
				{						
					break;
				}
				pr_file_p++;
			}
			pr_immediate._float *= sign;
			return;
		}
		else if (c == 'i')
		{
			pr_file_p++;
			pr_immediate_type = type_integer;
			pr_immediate._int = num*sign;
			return;
		}
		else break;
		pr_file_p++;
	}

	pr_immediate_type = type_float;
	pr_immediate._float = (float)(num*sign);
}

float PR_LexFloat (void)
{
	int	c;
	int	len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
		// only allow a . if the next isn't too...
	} while ((c >= '0' && c<= '9') || (c == '.'&&pr_file_p[1]!='.')); 

	pr_token[len] = 0;
	return (float)atof (pr_token);
}

/*
==============
PR_LexVector

Parses a single quoted vector
==============
*/
void PR_LexVector (void)
{
	int	i;
	
	pr_file_p++;

	if (*pr_file_p == '\\')
	{
		// extended characture constant
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_file_p++;
		switch(*pr_file_p)
		{
		case 'n':
			pr_immediate._float = '\n';
			break;
		case 'r':
			pr_immediate._float = '\r';
			break;
		case 't':
			pr_immediate._float = '\t';
			break;
		case '\'':
			pr_immediate._float = '\'';
			break;
		case '\"':
			pr_immediate._float = '\"';
			break;
		case '\\':
			pr_immediate._float = '\\';
			break;
		default:
			PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad characture constant");
			break;
		}
		if (*pr_file_p != '\'')
			PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad characture constant");
		pr_file_p++;
		return;
	}
	if (pr_file_p[1] == '\'')
	{
		// character constant
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_immediate._float = pr_file_p[0];
		pr_file_p+=2;
		return;
	}

	pr_token_type = tt_immediate;
	pr_immediate_type = type_vector;
	PR_LexWhitespace ();

	for (i = 0; i < 3; i++)
	{
		pr_immediate.vector[i] = PR_LexFloat ();
		PR_LexWhitespace ();
	}

	if (*pr_file_p != '\'')
		PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad vector");
	pr_file_p++;
}

/*
==============
PR_LexName

Parses an identifier
==============
*/
void PR_LexName (void)
{
	int	c;
	int	len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'  || (c >= '0' && c <= '9'));	

	pr_token[len] = 0;
	pr_token_type = tt_name;	
}

/*
==============
PR_LexPunctuation
==============
*/
void PR_LexPunctuation (void)
{
	int	i;
	int	len;
	char	*p;
	
	pr_token_type = tt_punct;
	
	for (i = 0; (p = pr_punctuation1[i]) != NULL; i++)
	{
		len = strlen(p);
		if (!strncmp(p, pr_file_p, len) )
		{
			strcpy (pr_token, pr_punctuation2[i]);
			if (p[0] == '{') pr_bracelevel++;
			else if (p[0] == '}') pr_bracelevel--;
			pr_file_p += len;
			return;
		}
	}
	PR_ParseError (ERR_UNKNOWNPUCTUATION, "Unknown punctuation");
}
		
/*
==============
PR_LexWhitespace
==============
*/
void PR_LexWhitespace (void)
{
	int		c;
	
	while (1)
	{
		// skip whitespace
		while ( (c = *pr_file_p) <= ' ')
		{
			if (c=='\n')
			{
				PR_NewLine (false);
				if (!pr_file_p) return;
			}
			if (c == 0) return;	// end of file
			pr_file_p++;
		}
		
		// skip // comments
		if (c=='/' && pr_file_p[1] == '/')
		{
			while (*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			PR_NewLine(false);
			pr_file_p++;
			continue;
		}
		
		// skip /* */ comments
		if (c=='/' && pr_file_p[1] == '*')
		{
			do
			{
				pr_file_p++;
				if (pr_file_p[0]=='\n')
					PR_NewLine(true);
				if (pr_file_p[1] == 0)
				{
					pr_file_p++;
					return;
				}
			} while (pr_file_p[-1] != '*' || pr_file_p[0] != '/');
			pr_file_p++;
			continue;
		}
		break; // a real character has been found
	}
}

//============================================================================

#define MAX_FRAMES		8192
char pr_framemodelname[64];
char pr_framemacros[MAX_FRAMES][16];
int pr_framemacrovalue[MAX_FRAMES];
int pr_nummacros, pr_oldmacros;
int pr_macrovalue;
int pr_savedmacro;

void PR_ClearGrabMacros (void)
{
	pr_oldmacros = pr_nummacros;
	pr_macrovalue = 0;
	pr_savedmacro = -1;
}

int PR_FindMacro (char *name)
{
	int	i;

	for (i = pr_nummacros - 1; i >= 0; i--)
	{
		if (!STRCMP (name, pr_framemacros[i]))
		{
			return pr_framemacrovalue[i];
		}
	}
	for (i = pr_nummacros - 1; i >= 0; i--)
	{
		if (!stricmp (name, pr_framemacros[i]))
		{
			PR_ParseWarning(WARN_CASEINSENSATIVEFRAMEMACRO, "Case insensative frame macro");
			return pr_framemacrovalue[i];
		}
	}
	return -1;
}

void PR_ExpandMacro(void)
{
	int i = PR_FindMacro(pr_token);

	if (i < 0) PR_ParseError (ERR_BADFRAMEMACRO, "Unknown frame macro $%s", pr_token);

	sprintf (pr_token,"%d", i);
	pr_token_type = tt_immediate;
	pr_immediate_type = type_float;
	pr_immediate._float = (float)i;
}

bool PR_SimpleGetToken (void)
{
	int	c;
	int	i = 0;

	// skip whitespace
	while ( (c = *pr_file_p) <= ' ')
	{
		if (c=='\n' || c == 0)
			return false;
		pr_file_p++;
	}
	if (pr_file_p[0] == '/')
	{
		if (pr_file_p[1] == '/')
		{	//comment alert
			while(*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			return false;
		}
		if (pr_file_p[1] == '*')
			return false;
	}

	while ( (c = *pr_file_p) > ' ' && c != ',' && c != ';' && c != ')' && c != '(' && c != ']')
	{
		pr_token[i] = c;
		i++;
		pr_file_p++;
	}
	pr_token[i] = 0;
	return i!=0;
}

void PR_MacroFrame(char *name, int value)
{
	int 	i;
	for (i = pr_nummacros - 1; i >= 0; i--)
	{
		if (!STRCMP (name, pr_framemacros[i]))
		{
			pr_framemacrovalue[i] = value;
			if (i >= pr_oldmacros)
				PR_ParseWarning(WARN_DUPLICATEMACRO, "Duplicate macro defined (%s)", pr_token);
			return;
		}
	}

	strcpy (pr_framemacros[pr_nummacros], name);
	pr_framemacrovalue[pr_nummacros] = value;
	pr_nummacros++;
	if (pr_nummacros >= MAX_FRAMES) PR_ParseError(ERR_TOOMANYFRAMEMACROS, "Too many frame macros defined");
}

void PR_ParseFrame (void)
{
	while (PR_SimpleGetToken ())
	{
		PR_MacroFrame(pr_token, pr_macrovalue++);
	}
}

/*
==============
PR_LexGrab

Deals with counting sequence numbers and replacing frame macros
==============
*/
void PR_LexGrab (void)
{	
	pr_file_p++;	// skip the $

	if (*pr_file_p <= ' ') PR_ParseError (ERR_BADFRAMEMACRO, "hanging $");
	PR_SimpleGetToken();
	if (!*pr_token) PR_ParseError (ERR_BADFRAMEMACRO, "hanging $");
	
	// check for $frame
	if (!STRCMP (pr_token, "frame") || !STRCMP (pr_token, "framesave"))
	{
		PR_ParseFrame ();
		PR_Lex ();
	}

	// ignore other known $commands - just for model/spritegen
	else if (!STRCMP (pr_token, "cd") || !STRCMP (pr_token, "origin") || !STRCMP (pr_token, "base") || !STRCMP (pr_token, "flags") || !STRCMP (pr_token, "scale") || !STRCMP (pr_token, "skin") )
	{	
		// skip to end of line
		while (PR_SimpleGetToken ());
		PR_Lex ();
	}
	else if (!STRCMP (pr_token, "flush"))
	{
		PR_ClearGrabMacros();
		while (PR_SimpleGetToken ());
		PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framevalue"))
	{
		PR_SimpleGetToken ();
		pr_macrovalue = atoi(pr_token);
		PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framerestore"))
	{
		PR_SimpleGetToken ();
		PR_ExpandMacro();
		pr_macrovalue = (int)pr_immediate._float;
		PR_Lex ();
	}
	else if (!STRCMP (pr_token, "modelname"))
	{
		int i;
		PR_SimpleGetToken ();

		if (*pr_framemodelname)
			PR_MacroFrame(pr_framemodelname, pr_macrovalue);

		strncpy(pr_framemodelname, pr_token, sizeof(pr_framemodelname)-1);
		pr_framemodelname[sizeof(pr_framemodelname)-1] = '\0';

		i = PR_FindMacro(pr_framemodelname);
		if (i) pr_macrovalue = i;
		else i = 0;
		PR_Lex ();
	}
	else PR_ExpandMacro (); // look for a frame name macro
}

bool PR_UndefineName(char *name)
{
	const_t	*c = Hash_Get(&compconstantstable, name);

	if(!c)
	{
		PR_ParseWarning(WARN_UNDEFNOTDEFINED, "Precompiler constant %s was not defined", name);
		return false;
	}

	Hash_Remove(&compconstantstable, name);
	return true;
}

const_t *PR_DefineName(char *name)
{
	int	i;
	const_t	*cnst;

	if (strlen(name) >= MAX_NAME || !*name)
		PR_ParseError(ERR_CONSTANTTOOLONG, "Compiler constant name length is too long or short");
	
	cnst = Hash_Get(&compconstantstable, name);
	if (cnst )
	{
		PR_ParseWarning(WARN_DUPLICATEDEFINITION, "Duplicate definition for Precompiler constant %s", name);
		Hash_Remove(&compconstantstable, name);
	}

	cnst = Qalloc(sizeof(const_t));
	cnst->used = false;
	cnst->numparams = 0;
	strcpy(cnst->name, name);
	cnst->namelen = strlen(name);
	*cnst->value = '\0';
	for (i = 0; i < MAX_PARMS; i++) cnst->params[i][0] = '\0';

	Hash_Add(&compconstantstable, cnst->name, cnst, Qalloc(sizeof(bucket_t)));

	if (!STRCMP(name, "OP_NODUP")) opt_noduplicatestrings = true;

	// group - optimize for a fast compiler
	if (!STRCMP(name, "OP_TIME"))
	{
		PR_UndefineName("OP_SIZE");
		PR_UndefineName("OP_SPEED");

		PR_UndefineName("OP_NODUP");
		PR_UndefineName("OP_COMP_ALL");
	}

	// group - optimize run speed
	if (!STRCMP(name, "OP_SPEED"))
	{
		PR_UndefineName("OP_SIZE");
		PR_UndefineName("OP_TIME");
		PR_UndefineName("OP_COMP_ALL");
	}

	// group - produce small output.
	if (!STRCMP(name, "OP_SIZE"))
	{
		PR_UndefineName("OP_SPEED");
		PR_UndefineName("OP_TIME");
		PR_DefineName("OP_NODUP");
		PR_DefineName("OP_COMP_ALL");
	}

	// group	- compress the output
	if (!STRCMP(name, "OP_COMP_ALL"))	
	{
		PR_DefineName("OP_COMP_STATEMENTS");
		PR_DefineName("OP_COMP_DEFS");
		PR_DefineName("OP_COMP_FIELDS");
		PR_DefineName("OP_COMP_FUNCTIONS");
		PR_DefineName("OP_COMP_STRINGS");
		PR_DefineName("OP_COMP_GLOBALS");
		PR_DefineName("OP_COMP_LINES");
		PR_DefineName("OP_COMP_TYPES");
	}
	return cnst;
}

void PR_Undefine(void)
{
	PR_SimpleGetToken ();
	PR_UndefineName(pr_token);
}

void PR_ConditionCompilation(void)
{
	char	*oldval;
	char	*d;
	char	*s;
	int	quote=false;
	const_t	*cnst;

	PR_SimpleGetToken ();

	if (!PR_SimpleGetToken()) PR_ParseError(ERR_NONAME, "No name defined for compiler constant");

	cnst = Hash_Get(&compconstantstable, pr_token);
	if (cnst)
	{
		oldval = cnst->value;
		Hash_Remove(&compconstantstable, pr_token);
	}
	else oldval = NULL;

	cnst = PR_DefineName(pr_token);

	if (*pr_file_p == '(')
	{
		s = pr_file_p + 1;
		while(*pr_file_p++)
		{
			if (*pr_file_p == ',')
			{
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				if (cnst->numparams > MAX_PARMS)
					PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAX_PARMS);
				pr_file_p++;
				s = pr_file_p;
			}
			if (*pr_file_p == ')')
			{
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				if (cnst->numparams > MAX_PARMS)
					PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAX_PARMS);
				pr_file_p++;
				break;
			}
		}
	}
	else cnst->numparams = -1;

	s = pr_file_p;
	d = cnst->value;
	while(*s == ' ' || *s == '\t') s++;
	while(1)
	{
		if (*s == '\r' || *s == '\n' || *s == '\0')
		{
			if (s[-1] == '\\');
			else if (s[-2] == '\\' && s[-1] == '\r' && s[0] == '\n');
			else break;
		}

		if (!quote && s[0]=='/'&&(s[1]=='/'||s[1]=='*')) break;
		if (*s == '\"') quote=!quote;
		*d = *s;
		d++, s++;
	}

	*d = '\0';
	d--;
	while(*d<= ' ' && d >= cnst->value) *d-- = '\0';

	if (strlen(cnst->value) >= sizeof(cnst->value))	//this is too late.
		PR_ParseError(ERR_CONSTANTTOOLONG, "Macro %s too long (%i not %i)", cnst->name, strlen(cnst->value), sizeof(cnst->value));

	if (oldval)
	{	
		// we always warn if it was already defined
		// we use different warning codes so that -Wno-mundane can be used to ignore identical redefinitions.
		if (strcmp(oldval, cnst->value))
			PR_ParseWarning(WARN_DUPLICATEPRECOMPILER, "Alternate precompiler definition of %s", pr_token);
		else PR_ParseWarning(WARN_IDENTICALPRECOMPILER, "Identical precompiler definition of %s", pr_token);
	}
}

int PR_CheakCompConst(void)
{
	char	*oldpr_file_p = pr_file_p;
	int	whitestart;
	const_t	*c;
	char	*end;

	for (end = pr_file_p; ; end++)
	{
		if (*end <= ' ') break;
		if (*end == ')') break;
		if (*end == '(') break;
		if (*end == '+') break;
		if (*end == '-') break;
		if (*end == '*') break;
		if (*end == '/') break;
		if (*end == '|') break;
		if (*end == '&') break;
		if (*end == '=') break;
		if (*end == '^') break;
		if (*end == '~') break;
		if (*end == '[') break;
		if (*end == ']') break;
		if (*end =='\"') break;
		if (*end == '{') break;
		if (*end == '}') break;
		if (*end == ';') break;
		if (*end == ':') break;
		if (*end == ',') break;
		if (*end == '.') break;
		if (*end == '#') break;
	}

	strncpy(pr_token, pr_file_p, end - pr_file_p);
	pr_token[end - pr_file_p] = '\0';

	c = Hash_Get(&compconstantstable, pr_token);

	if (c && !c->inside)
	{
		pr_file_p = oldpr_file_p + strlen(c->name);
		while(*pr_file_p == ' ' || *pr_file_p == '\t') pr_file_p++;
		if (c->numparams>=0)
		{
			if (*pr_file_p == '(')
			{
				int p;
				char *start;
				char buffer[1024];
				char *paramoffset[MAX_PARMS+1];
				int param=0;
				int plevel=0;

				pr_file_p++;
				while(*pr_file_p == ' ' || *pr_file_p == '\t') pr_file_p++;
				start = pr_file_p;

				while(1)
				{
					if (*pr_file_p == '(') plevel++;
					else if (!plevel && (*pr_file_p == ',' || *pr_file_p == ')'))
					{
						paramoffset[param++] = start;
						start = pr_file_p+1;
						if (*pr_file_p == ')')
						{
							*pr_file_p = '\0';
							pr_file_p++;
							break;
						}
						*pr_file_p = '\0';
						pr_file_p++;
						while(*pr_file_p == ' ' || *pr_file_p == '\t') pr_file_p++;
						if (param == MAX_PARMS)
							PR_ParseError(ERR_TOOMANYPARAMS, "Too many parameters in macro call");
					} else if (*pr_file_p == ')' ) plevel--;

					if (!*pr_file_p) PR_ParseError(ERR_EOF, "EOF on macro call");
					pr_file_p++;
				}
				if (param < c->numparams)
					PR_ParseError(ERR_TOOFEWPARAMS, "Not enough macro parameters");
				paramoffset[param] = start;

				*buffer = '\0';
				oldpr_file_p = pr_file_p;
				pr_file_p = c->value;

				while( 1 )
				{
					whitestart = p = strlen(buffer);
					while(*pr_file_p <= ' ') // copy across whitespace
					{
						if (!*pr_file_p) break;
						buffer[p++] = *pr_file_p++;
					}
					buffer[p] = 0;

					// if you ask for #a##b you will be shot. use #a #b instead, or chain macros.
					if (*pr_file_p == '#')
					{
						if (pr_file_p[1] == '#')
						{	
							// concatinate (skip out whitespace)
							buffer[whitestart] = '\0';
							pr_file_p+=2;
						}
						else
						{	// stringify
							pr_file_p++;
							SC_ParseWord(&pr_file_p);
							if (!pr_file_p) break;

							for (p = 0; p < param; p++)
							{
								if (!STRCMP(token, c->params[p]))
								{
									strcat(buffer, "\"");
									strcat(buffer, paramoffset[p]);
									strcat(buffer, "\"");
									break;
								}
							}
							if (p == param)
							{
								strcat(buffer, "#");
								strcat(buffer, token);
								PR_ParseWarning(0, "Stringification ignored");
							}
							continue;// already did this one
						}
					}
					SC_ParseWord(&pr_file_p);
					if (!pr_file_p) break;

					for (p = 0; p < param; p++)
					{
						if (!STRCMP(token, c->params[p]))
						{
							strcat(buffer, paramoffset[p]);
							break;
						}
					}
					if (p == param) strcat(buffer, token);
				}

				for (p = 0; p < param-1; p++)
					paramoffset[p][strlen(paramoffset[p])] = ',';
				paramoffset[p][strlen(paramoffset[p])] = ')';
				pr_file_p = oldpr_file_p;
				PR_IncludeChunk(buffer, true, NULL);
			}
			else PR_ParseError(ERR_TOOFEWPARAMS, "Macro without opening brace");
		}
		else PR_IncludeChunk(c->value, false, NULL);

		c->inside++;
		PR_Lex();
		c->inside--;
		return true;
	}

	// start of macros variables
	if (!strncmp(pr_file_p, "__TIME__", 8))
	{
		static char retbuf[128];

		time_t long_time;
		time( &long_time );
		strftime( retbuf, sizeof(retbuf), "\"%H:%M\"", localtime( &long_time ));

		pr_file_p = retbuf;
		PR_Lex();// translate the macro's value
		pr_file_p = oldpr_file_p + 8;

		return true;
	}
	if (!strncmp(pr_file_p, "__DATE__", 8))
	{
		static char retbuf[128];

		time_t long_time;
		time( &long_time );
		strftime( retbuf, sizeof(retbuf), "\"%a %d %b %Y\"", localtime( &long_time ));

		pr_file_p = retbuf;
		PR_Lex();	// translate the macro's value
		pr_file_p = oldpr_file_p + 8;

		return true;
	}
	if (!strncmp(pr_file_p, "__FILE__", 8))
	{		
		static char retbuf[256];
		sprintf(retbuf, "\"%s\"", strings + s_file);
		pr_file_p = retbuf;
		PR_Lex();	// translate the macro's value
		pr_file_p = oldpr_file_p + 8;

		return true;
	}
	if (!strncmp(pr_file_p, "__LINE__", 8))
	{
		static char retbuf[256];
		sprintf(retbuf, "\"%i\"", pr_source_line);
		pr_file_p = retbuf;
		PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p + 8;
		return true;
	}
	if (!strncmp(pr_file_p, "__FUNC__", 8))
	{		
		static char retbuf[256];
		sprintf(retbuf, "\"%s\"", pr_scope->name);
		pr_file_p = retbuf;
		PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;
		return true;
	}
	if (!strncmp(pr_file_p, "__NULL__", 8))
	{
		static char retbuf[256];
		sprintf(retbuf, "~0");
		pr_file_p = retbuf;
		PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p + 8;
		return true;
	}
	return false;
}

char *PR_CheakCompConstString(char *def)
{
	char	*s;
	const_t	*c;

	c = Hash_Get(&compconstantstable, def);
	if (c)	
	{
		s = PR_CheakCompConstString(c->value);
		return s;
	}
	return def;
}

const_t *PR_CheckCompConstDefined(char *def)
{
	return Hash_Get(&compconstantstable, def);
}

//============================================================================

/*
==============
PR_Lex

Advanced version of SC_ParseToken()
Sets pr_token, pr_token_type, and possibly pr_immediate and pr_immediate_type
==============
*/
void PR_Lex (void)
{
	int	c;

	pr_token[0] = 0;
	
	if (!pr_file_p) goto end_of_file;

	PR_LexWhitespace();

	if (!pr_file_p) goto end_of_file;
	c = *pr_file_p;
	if (!c) goto end_of_file;

	// handle quoted strings as a unit
	if (c == '\"')
	{
		PR_LexString ();
		return;
	}

	// handle quoted vectors as a unit
	if (c == '\'')
	{
		PR_LexVector ();
		return;
	}

	// if the first character is a valid identifier, parse until a non-id
	// character is reached
	if ( c == '~' || c == '%')	
	{
		// let's see which one we make into an operator first... possibly both...
		pr_file_p++;
		pr_token_type = tt_immediate;
		pr_immediate_type = type_integer;
		pr_immediate._int = PR_LexInteger ();
		return;
	}
	if ( c == '0' && pr_file_p[1] == 'x')
	{
		pr_token_type = tt_immediate;
		PR_LexNumber();
		return;
	}
	if ( (c == '.'&&pr_file_p[1] >='0' && pr_file_p[1] <= '9') || (c >= '0' && c <= '9') || ( c=='-' && pr_file_p[1]>='0' && pr_file_p[1] <='9') )
	{
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_immediate._float = PR_LexFloat ();
		return;
	}

	if (c == '#' && !(pr_file_p[1]=='-' || (pr_file_p[1]>='0' && pr_file_p[1] <='9')))
	{
		// hash and not number
		pr_file_p++;
		if (!PR_CheakCompConst())
		{
			if (!PR_SimpleGetToken()) strcpy(pr_token, "unknown");
			PR_ParseError(ERR_CONSTANTNOTDEFINED, "Explicit precompiler usage when not defined %s", pr_token);
		}
		else if (pr_token_type == tt_eof) PR_Lex();
		return;
	}
	
	if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' )
	{
		if (flag_hashonly || !PR_CheakCompConst()) PR_LexName (); // look for a macro.
		else if (pr_token_type == tt_eof) goto end_of_file;
		return;
	}
	if (c == '$')
	{
		PR_LexGrab ();
		return;
	}
	
	// parse symbol strings until a non-symbol is found
	PR_LexPunctuation ();
	return;
	
end_of_file:
	if (PR_UnInclude())
	{	
		PR_Lex();
		return;
	}
	pr_token_type = tt_eof;
}

void PR_ParsePrintDef (int type, def_t *def)
{
	if (pr_warning[type]) return;
	if (def->s_file) PR_Message ("%s:%i:    %s  is defined here\n", strings + def->s_file, def->s_line, def->name);
}

void PR_PrintScope (void)
{
	if (pr_scope)
	{
		if (errorscope != pr_scope) PR_Message ("in function %s (line %i),\n", pr_scope->name, pr_scope->s_line);
		errorscope = pr_scope;
	}
	else
	{
		if (errorscope) PR_Message ("at global scope,\n");
		errorscope = NULL;
	}
}

void PR_ResetErrorScope(void)
{
	errorscope = NULL;
}

/*
============
PR_ParseError
============
*/
void PR_ParseError (int errortype, char *error, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start( argptr, error );
	_vsnprintf(string, sizeof(string) - 1, error, argptr);
	va_end( argptr );

	if(errortype == ERR_INTERNAL)
	{
		// instead of sys error
		std.error( "internal error: %s\n", string );
	}
	else
	{
		PR_PrintScope();
		PR_Message("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);
		longjmp (pr_parse_abort, 1);
	}
}

void PR_ParseErrorPrintDef (int errortype, def_t *def, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	_vsnprintf (string, sizeof(string)-1, error, argptr);
	va_end (argptr);

	PR_PrintScope();
	PR_Message ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);

	PR_ParsePrintDef(WARN_ERROR, def);
	longjmp (pr_parse_abort, 1);
}

/*
============
PR_ParseWarning
============
*/
void PR_ParseWarning (int type, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (type < ERR_PARSEERRORS && pr_warning[type]) return;

	va_start (argptr,error);
	_vsnprintf (string,sizeof(string) - 1, error,argptr);
	va_end (argptr);

	PR_PrintScope();
	if(type == ERR_INTERNAL)
	{
		// instead of sys error
		pr_total_error_count++;
		std.error( "%s:%i: internal error C%i: %s\n", strings + s_file, pr_source_line, type, string );
	}
	else if (type > ERR_INTERNAL)
	{
		PR_Message("%s:%i: error C%i: %s\n", strings + s_file, pr_source_line, type, string);
		pr_total_error_count++;
		pr_error_count++;
	}
	else
	{
		PR_Message("%s:%i: warning C%i: %s\n", strings + s_file, pr_source_line, type, string);
		pr_warning_count++;
	}
}

void PR_Warning (int type, char *file, int line, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (pr_warning[type]) return;

	va_start (argptr,error);
	_vsnprintf (string,sizeof(string) - 1, error,argptr);
	va_end (argptr);

	PR_PrintScope();
	if (file) PR_Message ("%s(%i) : warning C%i: %s\n", file, line, type, string);
	else PR_Message ("warning C%i: %s\n", type, string);
	pr_warning_count++;
}

void PR_Message( char *message, ... )
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, message);
	_vsnprintf (string, sizeof(string) - 1, message, argptr);
	va_end (argptr);

	std.print( string );
}

/*
=============
PR_Expect

Issues an error if the current token isn't equal to string
Gets the next token
=============
*/
void PR_Expect (char *string)
{
	if(STRCMP(string, pr_token))
		PR_ParseError (ERR_EXPECTED, "expected %s, found %s",string, pr_token);
	PR_Lex ();
}

/*
=============
PR_Check

Returns true and gets the next token if the current token equals string
Returns false and does nothing otherwise
=============
*/
bool PR_CheckToken (char *string)
{
	if (STRCMP (string, pr_token))
		return false;
	
	PR_Lex ();
	return true;
}

bool PR_CheckName(char *string)
{
	if (STRCMP(string, pr_token))
		return false;

	PR_Lex ();
	return true;
}

bool PR_CheckKeyword(int keywordenabled, char *string)
{
	if (!keywordenabled) return false;
	if (STRCMP(string, pr_token))
		return false;
	PR_Lex ();
	return true;
}

/*
============
PR_ParseName

Checks to see if the current token is a valid name
============
*/
char *PR_ParseName (void)
{
	static char	ident[MAX_NAME];
	char		*ret;
	
	if (pr_token_type != tt_name) PR_ParseError (ERR_NOTANAME, "\"%s\" - not a name", pr_token);	
	if (strlen(pr_token) >= MAX_NAME-1) PR_ParseError (ERR_NAMETOOLONG, "name too long");

	strcpy (ident, pr_token);
	PR_Lex ();
	
	ret = Qalloc(strlen(ident) + 1);
	strcpy(ret, ident);
	return ret;
}

/*
============
PR_FindType

Returns a preexisting complex type that matches the parm, or allocates
a new one and copies it out.
============
*/
int typecmp(type_t *a, type_t *b)
{
	if (a == b) return 0;
	if (!a || !b) return 1; // different (^ and not both null)

	if (a->type != b->type) return 1;
	if (a->num_parms != b->num_parms) return 1;
	if (a->size != b->size) return 1;
	if (typecmp(a->aux_type, b->aux_type)) return 1;

	if (a->param || b->param)
	{
		a = a->param;
		b = b->param;

		while(a || b)
		{
			if(typecmp(a, b)) return 1;

			a = a->next;
			b = b->next;
		}
	}
	return 0;
}

type_t *PR_DuplicateType(type_t *in)
{
	type_t *out, *op, *ip;

	if (!in) return NULL;
	out = PR_NewType(in->name, in->type);
	out->aux_type = PR_DuplicateType(in->aux_type);
	out->param = PR_DuplicateType(in->param);
	ip = in->param;
	op = NULL;

	while(ip)
	{
		if (!op) out->param = op = PR_DuplicateType(ip);
		else op = (op->next = PR_DuplicateType(ip));
		ip = ip->next;
	}

	out->size = in->size;
	out->num_parms = in->num_parms;
	out->ofs = in->ofs;
	out->name = in->name;
	out->parentclass = in->parentclass;

	return out;
}

char *TypeName(type_t *type)
{
	static char	buffer[2][512];
	static int	op;
	char		*ret;

	op++;
	ret = buffer[op&1];

	if (type->type == ev_field)
	{
		type = type->aux_type;
		*ret++ = '.';
	}
	*ret = 0;

	if (type->type == ev_function)
	{
		strcat(ret, type->aux_type->name);
		strcat(ret, " (");
		type = type->param;

		while(type)
		{
			strcat(ret, type->name);
			type = type->next;
			if (type) strcat(ret, ", ");
		}
		strcat(ret, ")");
	}
	else if (type->type == ev_entity && type->parentclass)
	{
		ret = buffer[op&1];
		*ret = 0;
		strcat(ret, "class ");
		strcat(ret, type->name);
	}
	else strcpy(ret, type->name);

	return buffer[op&1];
}

type_t *PR_FindType (type_t *type)
{
	int t;
	for (t = 0; t < numtypeinfos; t++)
	{
		if (typecmp(&qcc_typeinfo[t], type))
			continue;
		return &qcc_typeinfo[t];
	}

	PR_ParseError(ERR_INTERNAL, "Error with type");
	return type;
}

type_t *TypeForName(char *name)
{
	int i;

	for (i = 0; i < numtypeinfos; i++)
	{
		if (!STRCMP(qcc_typeinfo[i].name, name))
			return &qcc_typeinfo[i];
	}

	return NULL;
}

/*
============
PR_SkipToSemicolon

For error recovery, also pops out of nested braces
============
*/
void PR_SkipToSemicolon (void)
{
	do
	{
		if (!pr_bracelevel && PR_CheckToken (";"))
			return;
		PR_Lex ();
	} while (pr_token_type != tt_eof);
}


/*
============
PR_ParseType

Parses a variable type, including field and functions types
============
*/
type_t *PR_ParseFunctionType (int newtype, type_t *returntype)
{
	type_t	*ftype, *ptype, *nptype;
	char	*name;
	int	definenames = !recursivefunctiontype;

	recursivefunctiontype++;
	ftype = PR_NewType(type_function->name, ev_function);

	ftype->aux_type = returntype;	// return type
	ftype->num_parms = 0;
	ptype = NULL;

	if (!PR_CheckToken (")"))
	{
		if (PR_CheckToken ("...")) ftype->num_parms = -1;	// variable args
		else
		{
			do
			{
				if (ftype->num_parms>=MAX_PARMS+MAX_PARMS_EXTRA)
					PR_ParseError(ERR_TOOMANYTOTALPARAMETERS, "Too many parameters. Sorry. (limit is %i)\n", MAX_PARMS+MAX_PARMS_EXTRA);

				if (PR_CheckToken ("..."))
				{
					ftype->num_parms = (ftype->num_parms * -1) - 1;
					break;
				}
				nptype = PR_ParseType(true);

				if (nptype->type == ev_void) break;
				if (!ptype)
				{
					ptype = nptype;
					ftype->param = ptype;
				}
				else
				{
					ptype->next = nptype;
					ptype = ptype->next;
				}
				if (STRCMP(pr_token, ",") && STRCMP(pr_token, ")"))
				{
					name = PR_ParseName ();
					if (definenames) strcpy (pr_parm_names[ftype->num_parms], name);
				}
				else if (definenames) strcpy (pr_parm_names[ftype->num_parms], "");
				ftype->num_parms++;
			} while (PR_CheckToken (","));
	          }
		PR_Expect (")");
	}

	recursivefunctiontype--;
	if (newtype) return ftype;
	return PR_FindType (ftype);
}

type_t *PR_PointerType (type_t *pointsto)
{
	type_t	*ptype;
	char	name[128];

	sprintf(name, "*%s", pointsto->name);
	ptype = PR_NewType(name, ev_pointer);
	ptype->aux_type = pointsto;

	return PR_FindType (ptype);
}

type_t *PR_FieldType (type_t *pointsto)
{
	type_t	*ptype;
	char	name[128];

	sprintf(name, "FIELD TYPE(%s)", pointsto->name);
	ptype = PR_NewType(name, ev_field);
	ptype->aux_type = pointsto;
	ptype->size = ptype->aux_type->size;

	return PR_FindType (ptype);
}

type_t *PR_ParseType (int newtype)
{
	type_t		*newparm;
	type_t		*newt;
	type_t		*type;
	char		*name;
	int		i;

	if (PR_CheckToken (".."))	//so we don't end up with the user specifying '. .vector blah'
	{
		newt = PR_NewType("FIELD TYPE", ev_field);
		newt->aux_type = PR_ParseType (false);
		newt->size = newt->aux_type->size;
		newt = PR_FindType (newt);

		type = PR_NewType("FIELD TYPE", ev_field);
		type->aux_type = newt;
		type->size = type->aux_type->size;

		if (newtype) return type;
		return PR_FindType (type);
	}
	if (PR_CheckToken ("."))
	{
		newt = PR_NewType("FIELD TYPE", ev_field);
		newt->aux_type = PR_ParseType (false);
		newt->size = newt->aux_type->size;

		if (newtype) return newt;
		return PR_FindType (newt);
	}

	name = PR_CheakCompConstString(pr_token);

	if (PR_CheckKeyword (keyword_class, "class"))
	{
		type_t	*fieldtype;
		char	membername[2048];
		char	*classname = PR_ParseName();

		newt = PR_NewType(classname, ev_entity);
		newt->size = type_entity->size;
		type = NULL;

		if (PR_CheckToken(":"))
		{
			char *parentname = PR_ParseName();
			newt->parentclass = TypeForName(parentname);
			if (!newt->parentclass)
				PR_ParseError(ERR_NOTANAME, "Parent class %s was not defined", parentname);
		}
		else newt->parentclass = type_entity;

		PR_Expect("{");
		if (PR_CheckToken(",")) PR_ParseError(ERR_NOTANAME, "member missing name");

		while (!PR_CheckToken("}"))
		{
			newparm = PR_ParseType(true);

			// we wouldn't be able to handle it.
			if (newparm->type == ev_struct || newparm->type == ev_union)
				PR_ParseError(ERR_INTERNAL, "Struct or union in class %s", classname);

			if (!PR_CheckToken(";"))
			{
				newparm->name = PR_CopyString(pr_token, opt_noduplicatestrings ) + strings;
				PR_Lex();
				if (PR_CheckToken("["))
				{
					type->next->size*=atoi(pr_token);
					PR_Lex();
					PR_Expect("]");
				}
				PR_CheckToken(";");
			}
			else newparm->name = PR_CopyString("", opt_noduplicatestrings )+strings;

			sprintf(membername, "%s::"MEMBERFIELDNAME, classname, newparm->name);
			fieldtype = PR_NewType(newparm->name, ev_field);
			fieldtype->aux_type = newparm;
			fieldtype->size = newparm->size;
			PR_GetDef(fieldtype, membername, pr_scope, 2, 1);

			newparm->ofs = 0; // newt->size;
			newt->num_parms++;

			if (type) type->next = newparm;
			else newt->param = newparm;
			type = newparm;
		}
		PR_Expect(";");
		return NULL;
	}
	if (PR_CheckKeyword (keyword_struct, "struct"))
	{
		newt = PR_NewType("struct", ev_struct);
		newt->size=0;
		PR_Expect("{");

		type = NULL;
		if (PR_CheckToken(",")) PR_ParseError(ERR_NOTANAME, "element missing name");

		newparm = NULL;
		while (!PR_CheckToken("}"))
		{
			if (PR_CheckToken(","))
			{
				if (!newparm) PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = PR_NewType(newparm->name, newparm->type);
			}
			else newparm = PR_ParseType(true);

			if (!PR_CheckToken(";"))
			{
				newparm->name = PR_CopyString(pr_token, opt_noduplicatestrings )+strings;
				PR_Lex();
				if (PR_CheckToken("["))
				{
					newparm->size*=atoi(pr_token);
					PR_Lex();
					PR_Expect("]");
				}
				PR_CheckToken(";");
			}
			else newparm->name = PR_CopyString("", opt_noduplicatestrings )+strings;
			newparm->ofs = newt->size;
			newt->size += newparm->size;
			newt->num_parms++;

			if (type) type->next = newparm;
			else newt->param = newparm;
			type = newparm;
		}
		return newt;
	}
	if (PR_CheckKeyword (keyword_union, "union"))
	{
		newt = PR_NewType("union", ev_union);
		newt->size = 0;
		PR_Expect("{");
		
		type = NULL;
		if (PR_CheckToken(",")) PR_ParseError(ERR_NOTANAME, "element missing name");
		newparm = NULL;

		while (!PR_CheckToken("}"))
		{
			if (PR_CheckToken(","))
			{
				if (!newparm) PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = PR_NewType(newparm->name, newparm->type);
			}
			else newparm = PR_ParseType(true);

			if (PR_CheckToken(";")) newparm->name = PR_CopyString("", opt_noduplicatestrings )+strings;
			else
			{
				newparm->name = PR_CopyString(pr_token, opt_noduplicatestrings )+strings;
				PR_Lex();
				PR_Expect(";");
			}

			newparm->ofs = 0;
			if (newparm->size > newt->size) newt->size = newparm->size;
			newt->num_parms++;
			
			if (type) type->next = newparm;
			else newt->param = newparm;
			type = newparm;
		}
		return newt;
	}

	type = NULL;
	for (i = 0; i < numtypeinfos; i++)
	{
		if (!STRCMP(qcc_typeinfo[i].name, name))
		{
			type = &qcc_typeinfo[i];
			break;
		}
	}

	if (i == numtypeinfos)
	{
		if (!*name) return NULL;
		PR_ParseError (ERR_NOTATYPE, "\"%s\" is not a type", name);
		type = type_float;	// shut up compiler warning
	}
	PR_Lex ();

	// this is followed by parameters. Must be a function.
	if (PR_CheckToken ("(")) return PR_ParseFunctionType(newtype, type);
	else
	{
		if (newtype) type = PR_DuplicateType(type);			
		return type;
	}
}