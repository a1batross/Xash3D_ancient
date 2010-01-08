//=======================================================================
//			Copyright XashXT Group 2007 ©
//			con_keys.c - console key events
//=======================================================================

#include "common.h"
#include "input.h"
#include "client.h"

typedef struct key_s
{
	bool	down;
	int	repeats;	// if > 1, it is autorepeating
	char	*binding;
} key_t;

typedef struct keyname_s
{
	char	*name;		// key name
	int	keynum;		// key number
	char	*binding;		// default bind
} keyname_t;

field_t	historyEditLines[COMMAND_HISTORY];
field_t	g_consoleField;
field_t	chatField;
key_t	keys[256];

int nextHistoryLine;// the last line in the history buffer, not masked
int historyLine;	// the line being displayed from history buffer will be <= nextHistoryLine
bool key_overstrikeMode;
bool anykeydown;
bool chat_team;

// auto-complete stuff
static field_t *completionField;
static const char *completionString;
static string shortestMatch;
static int matchCount;

keyname_t keynames[] =
{
	{"TAB",		K_TAB,		""		},
	{"ENTER",		K_ENTER,		""		},
	{"ESCAPE",	K_ESCAPE, 	"cancelselect"	}, // hardcoded
	{"SPACE",		K_SPACE,		"+moveup"		},
	{"BACKSPACE",	K_BACKSPACE,	""		},
	{"UPARROW",	K_UPARROW,	"+forward"	},
	{"DOWNARROW",	K_DOWNARROW,	"+back"		},
	{"LEFTARROW",	K_LEFTARROW,	"+left"		},
	{"RIGHTARROW",	K_RIGHTARROW,	"+right"		},
	{"ALT",		K_ALT,		"+strafe"		},
	{"CTRL",		K_CTRL,		"+attack"		},
	{"SHIFT",		K_SHIFT,		"+speed"		}, // replace with +attack2 ?
	{"COMMAND",	K_COMMAND,	""		},
	{"CAPSLOCK",	K_CAPSLOCK,	""		},
	{"F1",		K_F1,		"cmd help"	},
	{"F2",		K_F2,		"menu_savegame"	},
	{"F3",		K_F3,		"menu_loadgame"	},
	{"F4",		K_F4,		"menu_keys"	},
	{"F5",		K_F5,		"menu_startserver"	},
	{"F6",		K_F6,		"savequick"	},
	{"F7",		K_F7,		"loadquick"	},
	{"F8",		K_F8,		"stop"		},
	{"F9",		K_F9,		""		},
	{"F10",		K_F10,		"menu_quit"	},
	{"F11",		K_F11,		""		},
	{"F12",		K_F12,		"screenshot"	},
	{"INS",		K_INS,		""		},
	{"DEL",		K_DEL,		"+lookdown"	},
	{"PGDN",		K_PGDN,		"+lookup"		},
	{"PGUP",		K_PGUP,		""		},
	{"HOME",		K_HOME,		""		},
	{"END",		K_END,		"centerview"	},

	// mouse buttouns
	{"MOUSE1",	K_MOUSE1,		"+attack"		},
	{"MOUSE2",	K_MOUSE2,		"+atack2"		},
	{"MOUSE3",	K_MOUSE3,		""		},
	{"MOUSE4",	K_MOUSE4,		""		},
	{"MOUSE5",	K_MOUSE5,		""		},
	{"MWHEELUP",	K_MWHEELUP,	""		},
	{"MWHEELDOWN",	K_MWHEELDOWN,	""		},

	// digital keyboard
	{"KP_HOME",	K_KP_HOME,	""		},
	{"KP_UPARROW",	K_KP_UPARROW,	"+forward"	},
	{"KP_PGUP",	K_KP_PGUP,	""		},
	{"KP_LEFTARROW",	K_KP_LEFTARROW,	"+left"		},
	{"KP_5",		K_KP_5,		""		},
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW,	"+right"		},
	{"KP_END",	K_KP_END,		"centerview"	},
	{"KP_DOWNARROW",	K_KP_DOWNARROW,	"+back"		},
	{"KP_PGDN",	K_KP_PGDN,	"+lookup" 	},
	{"KP_ENTER",	K_KP_ENTER,	""		},
	{"KP_INS",	K_KP_INS,		""		},
	{"KP_DEL",	K_KP_DEL,		"+lookdown"	},
	{"KP_SLASH",	K_KP_SLASH,	""		},
	{"KP_MINUS",	K_KP_MINUS,	""		},
	{"KP_PLUS",	K_KP_PLUS,	""		},
	{"KP_NUMLOCK",	K_KP_NUMLOCK,	""		},
	{"KP_STAR",	K_KP_STAR,	""		},
	{"KP_EQUALS",	K_KP_EQUALS,	""		},
	{"PAUSE",		K_PAUSE,	"pause"			},

	// raw semicolon seperates commands
	{"SEMICOLON",	';',	""			},
	{NULL,		0,	NULL			},
};

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/

void Field_Clear( field_t *edit )
{
	memset(edit->buffer, 0, MAX_EDIT_LINE);
	edit->cursor = 0;
	edit->scroll = 0;
}

/*
===============
Field_CheckName

compare first argument with string
===============
*/
static bool Field_CheckName( const char *name )
{
	if(!com.stricmp( Cmd_Argv( 0 ), name ))
		return true;
	if(!com.stricmp( Cmd_Argv( 0 ), va("\\%s", name )))
		return true;
	return false;
}

/*
===============
FindMatches

===============
*/
static void FindMatches( const char *s, const char *unused1, const char *unused2, void *unused3 )
{
	int		i;

	if(com.strnicmp( s, completionString, com.strlen( completionString )))
		return;

	matchCount++;
	if ( matchCount == 1 )
	{
		com.strncpy( shortestMatch, s, sizeof( shortestMatch ));
		return;
	}

	// cut shortestMatch to the amount common with s
	for ( i = 0; s[i]; i++ )
	{
		if ( com.tolower(shortestMatch[i]) != com.tolower(s[i]))
			shortestMatch[i] = 0;
	}
}

/*
===============
PrintMatches
===============
*/
static void PrintMatches( const char *s, const char *unused1, const char *m, void *unused2 )
{
	if(!com.strnicmp( s, shortestMatch, com.strlen( shortestMatch )))
	if(m && *m) Msg( "    %s ^3\"%s\"\n", s, m );
	else Msg( "    %s\n", s ); // variable or command without description
}

static void keyConcatArgs( void )
{
	int	i;
	char	*arg;

	for ( i = 1; i < Cmd_Argc(); i++ )
	{
		com.strncat( completionField->buffer, " ", sizeof( completionField->buffer ));
		arg = Cmd_Argv( i );
		while (*arg)
		{
			if (*arg == ' ')
			{
				com.strncat( completionField->buffer, "\"", sizeof( completionField->buffer ));
				break;
			}
			arg++;
		}
		com.strncat( completionField->buffer, Cmd_Argv( i ), sizeof( completionField->buffer ));
		if (*arg == ' ') com.strncat( completionField->buffer, "\"", sizeof( completionField->buffer ));
	}
}

static void ConcatRemaining( const char *src, const char *start )
{
	char *str;

	str = com.strstr(src, start);
	if (!str)
	{
		keyConcatArgs();
		return;
	}

	str += com.strlen(start);
	com.strncat( completionField->buffer, str, sizeof( completionField->buffer ));
}

/*
===============
Field_CompleteCommand

perform Tab expansion
===============
*/
void Field_CompleteCommand( field_t *field )
{
	field_t		temp;
	string		filename;
	autocomplete_list_t	*list;

	completionField = field;

	// only look at the first token for completion purposes
	Cmd_TokenizeString( completionField->buffer );

	completionString = Cmd_Argv( 0 );
	if ( completionString[0] == '\\' || completionString[0] == '/' )
		completionString++;
	
	matchCount = 0;
	shortestMatch[0] = 0;

	if(!com.strlen( completionString)) return;

	Cmd_LookupCmds( NULL, NULL, FindMatches );
	Cvar_LookupVars( 0, NULL, NULL, FindMatches );

	if ( matchCount == 0 ) return; // no matches
	Mem_Copy(&temp, completionField, sizeof(field_t));

	if(Cmd_Argc() == 2)
	{
		bool result = false;

		// autocomplete second arg
		for( list = cmd_list; list->name; list++ )
		{
			if(Field_CheckName( list->name ))
			{
				result = list->func( Cmd_Argv(1), filename, MAX_STRING ); 
				break;
			}
		}
		if( result )
		{         
			com.sprintf( completionField->buffer, "%s %s", Cmd_Argv(0), filename ); 
			completionField->cursor = com.strlen( completionField->buffer );
			return;
		}
	}  

	if( matchCount == 1 )
	{
		com.sprintf( completionField->buffer, "\\%s", shortestMatch );
		if ( Cmd_Argc() == 1 ) com.strncat( completionField->buffer, " ", sizeof( completionField->buffer ));
		else ConcatRemaining( temp.buffer, completionString );
		completionField->cursor = com.strlen( completionField->buffer );
		return;
	}

	// multiple matches, complete to shortest
	com.sprintf( completionField->buffer, "\\%s", shortestMatch );
	completionField->cursor = com.strlen( completionField->buffer );
	ConcatRemaining( temp.buffer, completionString );

	Msg( "]%s\n", completionField->buffer );

	// run through again, printing matches
	Cmd_LookupCmds( NULL, NULL, PrintMatches );
	Cvar_LookupVars( 0, NULL, NULL, PrintMatches );
}

/*
===================
Field_Draw

Handles horizontal scrolling and cursor blinking
x, y, amd width are in pixels
===================
*/
void Field_VariableSizeDraw( field_t *edit, int x, int y, int width, int size, bool showCursor )
{
	int	len;
	int	drawLen;
	int	prestep;
	int	i, cursorChar;
	char	str[MAX_SYSPATH];

	drawLen = edit->widthInChars;
	len = com.strlen( edit->buffer ) + 1;

	// guarantee that cursor will be visible
	if ( len <= drawLen ) prestep = 0;
	else
	{
		if ( edit->scroll + drawLen > len )
		{
			edit->scroll = len - drawLen;
			if ( edit->scroll < 0 ) edit->scroll = 0;
		}
		prestep = edit->scroll;
	}

	if ( prestep + drawLen > len ) drawLen = len - prestep;

	// extract <drawLen> characters from the field at <prestep>
	if ( drawLen >= MAX_SYSPATH ) Host_Error("drawLen >= MAX_SYSPATH" );

	Mem_Copy( str, edit->buffer + prestep, drawLen );
	str[ drawLen ] = 0;

	// draw it
	if ( size == SMALLCHAR_WIDTH )
	{
		rgba_t	color = { 255, 255, 255, 255 };

		SCR_DrawSmallStringExt( x, y, str, color, false );
	}
	else SCR_DrawBigString( x, y, str, 255 ); // draw big string with drop shadow

	// draw the cursor
	if( !showCursor ) return;
	if((int)( cls.realtime>>8 ) & 1 )
		return; // off blink

	if( key_overstrikeMode ) cursorChar = 11;
	else cursorChar = 95;

	i = drawLen - (com.cstrlen(str) + 1);

	if( size == SMALLCHAR_WIDTH )
		SCR_DrawSmallChar( x + (edit->cursor - prestep - i) * size, y, cursorChar );
	else
	{
		str[0] = cursorChar;
		str[1] = 0;
		SCR_DrawBigString( x + ( edit->cursor - prestep - i ) * size, y, str, 255 );
	}
}

void Field_Draw( field_t *edit, int x, int y, int width, bool showCursor ) 
{
	Field_VariableSizeDraw( edit, x, y, width, SMALLCHAR_WIDTH, showCursor );
}

void Field_BigDraw( field_t *edit, int x, int y, int width, bool showCursor ) 
{
	Field_VariableSizeDraw( edit, x, y, width, BIGCHAR_WIDTH, showCursor );
}

/*
================
Field_Paste
================
*/
void Field_Paste( field_t *edit )
{
	char	*cbd;
	int	pasteLen, i;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) return;

	// send as if typed, so insert / overstrike works properly
	pasteLen = com.strlen( cbd );
	for ( i = 0 ; i < pasteLen ; i++ ) Field_CharEvent( edit, cbd[i] );
	Mem_Free( cbd );
}

/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
void Field_KeyDownEvent( field_t *edit, int key )
{
	int		len;

	// shift-insert is paste
	if((( key == K_INS ) || ( key == K_KP_INS )) && keys[K_SHIFT].down )
	{
		Field_Paste( edit );
		return;
	}

	len = com.strlen( edit->buffer );

	if ( key == K_DEL )
	{
		if ( edit->cursor < len )
		{
			memmove( edit->buffer + edit->cursor, edit->buffer + edit->cursor + 1, len - edit->cursor );
		}
		return;
	}
	if ( key == K_BACKSPACE )
	{
		if ( edit->cursor > 0 )
		{
			memmove( edit->buffer + edit->cursor - 1, edit->buffer + edit->cursor, len - edit->cursor + 1 );
			edit->cursor--;
			if ( edit->scroll ) edit->scroll--;
		}
		return;
	}
	if ( key == K_RIGHTARROW ) 
	{
		if ( edit->cursor < len ) edit->cursor++;
		if ( edit->cursor >= edit->scroll + edit->widthInChars && edit->cursor <= len )
			edit->scroll++;
		return;
	}
	if ( key == K_LEFTARROW ) 
	{
		if ( edit->cursor > 0 ) edit->cursor--;
		if ( edit->cursor < edit->scroll ) edit->scroll--;
		return;
	}
	if ( key == K_HOME || ( com.tolower(key) == 'a' && keys[K_CTRL].down ))
	{
		edit->cursor = 0;
		return;
	}
	if ( key == K_END || ( com.tolower(key) == 'e' && keys[K_CTRL].down ))
	{
		edit->cursor = len;
		return;
	}
	if ( key == K_INS )
	{
		key_overstrikeMode = !key_overstrikeMode;
		return;
	}
}

/*
==================
Field_CharEvent
==================
*/
void Field_CharEvent( field_t *edit, int ch )
{
	int		len;

	if ( ch == 'v' - 'a' + 1 )
	{
		// ctrl-v is paste
		Field_Paste( edit );
		return;
	}
	if ( ch == 'c' - 'a' + 1)
	{
		// ctrl-c clears the field
		Field_Clear( edit );
		return;
	}
	len = com.strlen( edit->buffer );

	if ( ch == 'a' - 'a' + 1 )
	{
		// ctrl-a is home
		edit->cursor = 0;
		edit->scroll = 0;
		return;
	}
	if ( ch == 'e' - 'a' + 1 )
	{
		// ctrl-e is end
		edit->cursor = len;
		edit->scroll = edit->cursor - edit->widthInChars;
		return;
	}

	// ignore any other non printable chars
	if ( ch < 32 ) return;

	if ( key_overstrikeMode )
	{	
		if ( edit->cursor == MAX_EDIT_LINE - 1 ) return;
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}
	else
	{
		// insert mode
		if ( len == MAX_EDIT_LINE - 1 ) return; // all full
		memmove( edit->buffer + edit->cursor + 1, edit->buffer + edit->cursor, len + 1 - edit->cursor );
		edit->buffer[edit->cursor] = ch;
		edit->cursor++;
	}
	if( edit->cursor >= edit->widthInChars ) edit->scroll++;
	if( edit->cursor == len + 1) edit->buffer[edit->cursor] = 0;
}

/*
=============================================================================

CONSOLE LINE EDITING

==============================================================================
*/

/*
====================
Key_Console

Handles history and console scrollback
====================
*/
void Key_Console( int key )
{
	// ctrl-L clears screen
	if( key == 'l' && keys[K_CTRL].down )
	{
		Cbuf_AddText ("clear\n");
		return;
	}

	// enter finishes the line
	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		// if not in the game explicitly prepent a slash if needed
		if ( cls.state != ca_active && g_consoleField.buffer[0] != '\\' && g_consoleField.buffer[0] != '/' )
		{
			char	temp[MAX_SYSPATH];

			com.strncpy( temp, g_consoleField.buffer, sizeof( temp ));
			com.sprintf( g_consoleField.buffer, "\\%s", temp );
			g_consoleField.cursor++;
		}

		// backslash text are commands, else chat
		if( g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/' )
			Cbuf_AddText( g_consoleField.buffer + 1 ); // skip backslash
		else Cbuf_AddText (g_consoleField.buffer); // valid command
		Cbuf_AddText ("\n");
		Msg( ">%s\n", g_consoleField.buffer ); // echo to console

		// copy line to history buffer
		historyEditLines[nextHistoryLine % COMMAND_HISTORY] = g_consoleField;
		nextHistoryLine++;
		historyLine = nextHistoryLine;

		Field_Clear( &g_consoleField );
		g_consoleField.widthInChars = g_console_field_width;

		if( cls.state == ca_disconnected )
		{
			// force an update, because the command may take some time
			SCR_UpdateScreen ();
		}
		return;
	}

	// command completion
	if( key == K_TAB )
	{
		Field_CompleteCommand( &g_consoleField );
		return;
	}

	// command history (ctrl-p ctrl-n for unix style)
	if(( key == K_MWHEELUP && keys[K_SHIFT].down ) || ( key == K_UPARROW ) || ( key == K_KP_UPARROW ) || (( com.tolower(key) == 'p' ) && keys[K_CTRL].down ))
	{
		if ( nextHistoryLine - historyLine < COMMAND_HISTORY && historyLine > 0 )
		{
			historyLine--;
		}
		g_consoleField = historyEditLines[ historyLine % COMMAND_HISTORY ];
		return;
	}
	if ( (key == K_MWHEELDOWN && keys[K_SHIFT].down) || ( key == K_DOWNARROW ) || ( key == K_KP_DOWNARROW ) || (( com.tolower(key) == 'n' ) && keys[K_CTRL].down ))
	{
		if (historyLine == nextHistoryLine) return;
		historyLine++;
		g_consoleField = historyEditLines[ historyLine % COMMAND_HISTORY ];
		return;
	}

	// console scrolling
	if ( key == K_PGUP )
	{
		Con_PageUp();
		return;
	}

	if( key == K_PGDN )
	{
		Con_PageDown();
		return;
	}

	if( key == K_MWHEELUP )
	{
		Con_PageUp();
		if( keys[K_CTRL].down )
		{
			Con_PageUp();
			Con_PageUp();
		}
		return;
	}

	if( key == K_MWHEELDOWN )
	{	
		Con_PageDown();
		if( keys[K_CTRL].down )
		{	
			Con_PageDown();
			Con_PageDown();
		}
		return;
	}

	// ctrl-home = top of console
	if( key == K_HOME && keys[K_CTRL].down )
	{
		Con_Top();
		return;
	}

	// ctrl-end = bottom of console
	if( key == K_END && keys[K_CTRL].down )
	{
		Con_Bottom();
		return;
	}

	// pass to the normal editline routine
	Field_KeyDownEvent( &g_consoleField, key );
}

/*
===================
Key_IsDown
===================
*/
bool Key_IsDown( int keynum )
{
	if ( keynum == -1 )
		return false;
	return keys[keynum].down;
}

/*
===================
Key_GetBind
===================
*/
char *Key_IsBind( int keynum )
{
	if( keynum == -1 || !keys[keynum].binding )
		return NULL;
	return keys[keynum].binding;
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keys[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.

0x11 will be interpreted as raw hex, which will allow new controlers

to be configured even if they don't have defined names.
===================
*/
int Key_StringToKeynum( const char *str )
{
	keyname_t		*kn;
	
	if( !str || !str[0] ) return -1;
	if( !str[1] ) return str[0];

	// check for hex code
	if( str[0] == '0' && str[1] == 'x' && com.strlen( str ) == 4 )
	{
		int	n1, n2;
		
		n1 = str[2];
		if( n1 >= '0' && n1 <= '9' )
		{
			n1 -= '0';
		}
		else if( n1 >= 'a' && n1 <= 'f' )
		{
			n1 = n1 - 'a' + 10;
		}
		else n1 = 0;

		n2 = str[3];
		if( n2 >= '0' && n2 <= '9' )
		{
			n2 -= '0';
		}
		else if( n2 >= 'a' && n2 <= 'f' )
		{
			n2 = n2 - 'a' + 10;
		}
		else n2 = 0;

		return n1 * 16 + n2;
	}

	// scan for a text match
	for( kn = keynames; kn->name; kn++ )
	{
		if( !com.stricmp( str,kn->name ))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, a K_* name, or a 0x11 hex string) for the
given keynum.
===================
*/
char *Key_KeynumToString( int keynum )
{
	keyname_t		*kn;	
	static char	tinystr[5];
	int		i, j;

	if ( keynum == -1 ) return "<KEY NOT FOUND>";
	if ( keynum < 0 || keynum > 255 ) return "<OUT OF RANGE>";

	// check for printable ascii (don't use quote)
	if( keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';' )
	{
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	// check for a key string
	for( kn = keynames; kn->name; kn++ )
	{
		if( keynum == kn->keynum )
			return kn->name;
	}

	// make a hex string
	i = keynum >> 4;
	j = keynum & 15;

	tinystr[0] = '0';
	tinystr[1] = 'x';
	tinystr[2] = i > 9 ? i - 10 + 'a' : i + '0';
	tinystr[3] = j > 9 ? j - 10 + 'a' : j + '0';
	tinystr[4] = 0;

	return tinystr;
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding( int keynum, char *binding )
{
	if( keynum == -1 ) return;

	// free old bindings
	if( keys[keynum].binding )
	{
		Mem_Free( keys[keynum].binding );
		keys[keynum].binding = NULL;
	}
		
	// allocate memory for new binding
	keys[keynum].binding = copystring( binding );
}


/*
===================
Key_GetBinding
===================
*/
char *Key_GetBinding( int keynum )
{
	if( keynum == -1 )
		return "";
	return keys[keynum].binding;
}

/* 
===================
Key_GetKey
===================
*/
int Key_GetKey( const char *binding )
{
	int	i;

	if( !binding ) return -1;

	for( i = 0; i < 256; i++ )
	{
		if( keys[i].binding && !com.stricmp( binding, keys[i].binding ))
			return i;
	}
	return -1;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f( void )
{
	int	b;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: unbind <key> : remove commands from a key\n" );
		return;
	}
	
	b = Key_StringToKeynum( Cmd_Argv( 1 ));
	if( b == -1 )
	{
		Msg( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ));
		return;
	}
	Key_SetBinding( b, "" );
}

/*
===================
Key_Unbindall_f
===================
*/
void Key_Unbindall_f( void )
{
	int	i;
	
	for( i = 0; i < 256; i++ )
	{
		if( keys[i].binding )
			Key_SetBinding( i, "" );
	}
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f( void )
{
	int	i, c, b;
	char	cmd[1024];
	
	c = Cmd_Argc();

	if( c < 2 )
	{
		Msg( "Usage: bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ));
	if( b == -1 )
	{
		Msg( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ));
		return;
	}

	if( c == 2 )
	{
		if( keys[b].binding )
			Msg( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keys[b].binding );
		else Msg( "\"%s\" is not bound\n", Cmd_Argv( 1 ));
		return;
	}
	
	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for( i = 2; i < c; i++ )
	{
		com.strcat( cmd, Cmd_Argv( i ));
		if( i != ( c - 1 )) com.strcat( cmd, " " );
	}
	Key_SetBinding( b, cmd );
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( file_t *f )
{
	int	i;

	if( !f ) return;
	FS_Printf( f, "unbindall\n" );

	for( i = 0; i < 256; i++ )
	{
		if( keys[i].binding && keys[i].binding[0] )
			FS_Printf( f, "bind %s \"%s\"\n", Key_KeynumToString(i), keys[i].binding );
	}
}


/*
============
Key_Bindlist_f

============
*/
void Key_Bindlist_f( void )
{
	int	i;

	for( i = 0; i < 256; i++ )
	{
		if( keys[i].binding && keys[i].binding[0] )
			Msg( "%s \"%s\"\n", Key_KeynumToString( i ), keys[i].binding );
	}
}

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/
/*
===================
Key_Init
===================
*/
void Key_Init( void )
{
	keyname_t	*kn;

	// register our functions
	Cmd_AddCommand( "bind", Key_Bind_f, "binds a command to the specified key in bindmap" );
	Cmd_AddCommand( "unbind", Key_Unbind_f, "removes a command on the specified key in bindmap" );
	Cmd_AddCommand( "unbindall", Key_Unbindall_f, "removes all commands from all keys in bindmap" );
	Cmd_AddCommand( "bindlist", Key_Bindlist_f, "display current key bindings" );
	Cmd_AddCommand( "makehelp", Key_EnumCmds_f, "write help.txt that contains all console cvars and cmds" ); 

	// setup hardcode binding. "unbindall" from keys.rc will be reset it
	for( kn = keynames; kn->name; kn++ ) Key_SetBinding( kn->keynum, kn->binding ); 
}

/*
===================
Key_AddKeyUpCommands
===================
*/
void Key_AddKeyUpCommands( int key, char *kb )
{
	int	i;
	char	button[1024], *buttonPtr;
	char	cmd[1024];
	bool	keyevent;

	if( !kb ) return;
	keyevent = false;
	buttonPtr = button;

	for( i = 0; ; i++ )
	{
		if( kb[i] == ';' || !kb[i] )
		{
			*buttonPtr = '\0';
			if( button[0] == '+' )
			{
				// button commands add keynum as a parm
				com.sprintf( cmd, "-%s %i\n", button+1, key );
				Cbuf_AddText( cmd );
				keyevent = true;
			}
			else
			{
				if( keyevent )
				{
					// down-only command
					Cbuf_AddText( button );
					Cbuf_AddText( "\n" );
				}
			}

			buttonPtr = button;
			while(( kb[i] <= ' ' || kb[i] == ';' ) && kb[i] != 0 )
				i++;
		}
		*buttonPtr++ = kb[i];
		if( !kb[i] ) break;
	}
}

/*
===================
CL_KeyEvent

Called by the system for both key up and key down events
===================
*/
void Key_Event( int key, bool down, int time )
{
	char	*kb;
	char	cmd[1024];

	// update auto-repeat status and BUTTON_ANY status
	keys[key].down = down;

	if( down )
	{
		keys[key].repeats++;
		if( keys[key].repeats == 1 ) anykeydown++;
	}
	else
	{
		keys[key].repeats = 0;
		anykeydown--;
		if( anykeydown < 0 ) anykeydown = 0;
	}

	// console key is hardcoded, so the user can never unbind it
	if( key == '`' || key == '~' )
	{
		if( !down ) return;
    		Con_ToggleConsole_f();
		return;
	}

	// escape is always handled special
	if( key == K_ESCAPE && down )
	{
		switch( cls.key_dest )
		{
		case key_game:
			if( cls.state == ca_cinematic )
				SCR_StopCinematic();
			else UI_SetActiveMenu( UI_MAINMENU );
			cls.key_dest = key_menu;
			return;
		case key_console:
			if( cls.state == ca_active )
			{
				cls.key_dest = key_game;
			}
			else
			{
				UI_SetActiveMenu( UI_MAINMENU );
				return; // don't pass Esc into menu
			}
			break;
		case key_menu:
			UI_KeyEvent( key, true );
			return;
		default:
			MsgDev( D_ERROR, "Key_Event: bad cls.key_dest\n" );
			return;
		}
	}

	if( cls.key_dest == key_menu )
	{
		UI_KeyEvent( key, down );
		return;
	}

	// key up events only perform actions if the game key binding is
	// a button command (leading + sign).  These will be processed even in
	// console mode and menu mode, to keep the character from continuing 
	// an action started before a mode switch.
	if( !down )
	{
		kb = keys[key].binding;

		Key_AddKeyUpCommands( key, kb );
		return;
	}

	// distribute the key down event to the apropriate handler
	if( cls.key_dest == key_game )
	{
		// send the bound action
		kb = keys[key].binding;
		if( !kb )
		{
			if( key >= 200 )
				Msg( "%s is unbound, use controls menu to set.\n", Key_KeynumToString( key ));
		}
		else if( kb[0] == '+' )
		{	
			int	i;
			char	button[1024], *buttonPtr;

			for( i = 0, buttonPtr = button; ; i++ )
			{
				if( kb[i] == ';' || !kb[i] )
				{
					*buttonPtr = '\0';
					if( button[0] == '+' )
					{
						// button commands add keynum and time as parms so that multiple
						// sources can be discriminated and subframe corrected
						com.sprintf( cmd, "%s %i %i\n", button, key, time );
						Cbuf_AddText( cmd );
					}
					else
					{
						// down-only command
						Cbuf_AddText( button );
						Cbuf_AddText( "\n" );
					}

					buttonPtr = button;
					while (( kb[i] <= ' ' || kb[i] == ';' ) && kb[i] != 0 )
						i++;
				}
				*buttonPtr++ = kb[i];
				if ( !kb[i] ) break;
			}
		}
		else
		{
			// down-only command
			Cbuf_AddText( kb );
			Cbuf_AddText( "\n" );
		}
	}
	else if( cls.key_dest == key_console )
	{
		Key_Console( key );
	}
}


/*
===================
CL_CharEvent

Normal keyboard characters, already shifted / capslocked / etc
===================
*/
void CL_CharEvent( int key )
{
	// the console key should never be used as a char
	if( key == '`' || key == '~' ) return;

	// distribute the key down event to the apropriate handler
	if( cls.key_dest == key_console || cls.state == ca_disconnected )
	{
		Field_CharEvent( &g_consoleField, key );
	}
}

/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent( int mx, int my )
{
	if( UI_IsVisible( ))
	{
		// if the menu is visible, move the menu cursor
		UI_MouseMove( mx, my );
	}
	else if( cls.key_dest != key_console )
	{
		// otherwise passed into client.dll
		clgame.dllFuncs.pfnMouseEvent( mx, my );
	}
}

/*
=========
Key_SetKeyDest
=========
*/
void Key_SetKeyDest( int key_dest )
{
	switch( key_dest )
	{
	case key_game:
		cls.key_dest = key_game;
		break;
	case key_menu:
		cls.key_dest = key_menu;
		break;
	case key_console:
		cls.key_dest = key_console;
		break;
	default:
		Host_Error( "Key_SetKeyDest: wrong destination (%i)\n", key_dest );
		break;
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates( void )
{
	int	i;

	anykeydown = false;
	for ( i = 0; i < 256; i++ )
	{
		if( keys[i].down ) Key_Event( i, false, 0 );
		keys[i].down = 0;
		keys[i].repeats = 0;
	}
}