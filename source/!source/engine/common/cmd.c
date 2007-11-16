/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// cmd.c -- Quake script command processing module

#include "engine.h"

void Cmd_ForwardToServer (void);

//=============================================================================

#define MAX_CMD_BUFFER	16384
#define MAX_CMD_LINE	1024

typedef struct
{
	byte		*data;
	int		maxsize;
	int		cursize;
} cmd_t;

int	cmd_wait;
cmd_t	cmd_text;
byte	cmd_text_buf[MAX_CMD_BUFFER];
char	**fs_argv;
int	fs_argc;

/*
=============================================================================

			COMMAND BUFFER

=============================================================================
*/
/*
============
Cbuf_Init
============
*/
void Cbuf_Init (int argc, char **argv )
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
	fs_argc = argc;
	fs_argv = argv;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText(const char *text)
{
	int	l;

	l = strlen(text);
	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		MsgDev(D_WARN, "Cbuf_AddText: overflow\n");
		return;
	}
	Mem_Copy(&cmd_text.data[cmd_text.cursize], (char *)text, l);
	cmd_text.cursize += l;
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (const char *text)
{
	int	i, len;

	len = strlen( text ) + 1;
	if ( len + cmd_text.cursize > cmd_text.maxsize )
	{
		MsgDev(D_WARN,"Cbuf_InsertText overflowed\n" );
		return;
	}

	// move the existing command text
	for ( i = cmd_text.cursize - 1; i >= 0; i-- )
	{
		cmd_text.data[i + len] = cmd_text.data[i];
	}

	// copy the new text in
	Mem_Copy( cmd_text.data, (char *)text, len - 1 );
	cmd_text.data[ len - 1 ] = '\n'; // add a \n
	cmd_text.cursize += len;
}

/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText (int exec_when, const char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		if (text && strlen(text))
			Cmd_ExecuteString(text);
		else Cbuf_Execute();
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		MsgWarn("Cbuf_ExecuteText: bad execute target\n");
		break;
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int	i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int	quotes;

	while (cmd_text.cursize)
	{
		if( cmd_wait )
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait--;
			break;
		}

		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"') quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break; // don't break if inside a quoted string
			if (text[i] == '\n' || text[i] == '\r' ) break;
		}

		if( i >= (MAX_CMD_LINE - 1)) i = MAX_CMD_LINE - 1;
		Mem_Copy (line, text, i);
		line[i] = 0;
		
		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.cursize) cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line);		
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/
/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f( void )
{
	int	i, j, l = 0;
	char	build[MAX_INPUTLINE]; // this is for all commandline options combined (and is bounds checked)

	if(Cmd_Argc() != 1)
	{
		Msg("stuffcmds : execute command line parameters\n");
		return;
	}

	// no reason to run the commandline arguments twice
	if(host.stuffcmdsrun) return;

	host.stuffcmdsrun = true;
	build[0] = 0;

	for (i = 0; i < fs_argc; i++)
	{
		if (fs_argv[i] && fs_argv[i][0] == '+' && (fs_argv[i][1] < '0' || fs_argv[i][1] > '9') && l + strlen(fs_argv[i]) - 1 <= sizeof(build) - 1)
		{
			j = 1;
			while (fs_argv[i][j]) build[l++] = fs_argv[i][j++];

			i++;
			for ( ; i < fs_argc; i++)
			{
				if (!fs_argv[i]) continue;
				if ((fs_argv[i][0] == '+' || fs_argv[i][0] == '-') && (fs_argv[i][1] < '0' || fs_argv[i][1] > '9'))
					break;
				if (l + strlen(fs_argv[i]) + 4 > sizeof(build) - 1)
					break;
				build[l++] = ' ';
				if (strchr(fs_argv[i], ' ')) build[l++] = '\"';
				for (j = 0; fs_argv[i][j]; j++) build[l++] = fs_argv[i][j];
				if (strchr(fs_argv[i], ' ')) build[l++] = '\"';
			}
			build[l++] = '\n';
			i--;
		}
	}

	// now terminate the combined string and prepend it to the command buffer
	// we already reserved space for the terminator
	build[l++] = 0;
	Cbuf_InsertText(build);
}

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
void Cmd_Wait_f (void)
{
	if(Cmd_Argc() == 2)
	{
		cmd_wait = atoi( Cmd_Argv( 1 ) );
	}
	else
	{
		cmd_wait = 1;
	}
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f, rcpath[MAX_QPATH];
	int	len;

	if (Cmd_Argc () != 2)
	{
		Msg("exec <filename> : execute a script file\n");
		return;
	}

	sprintf(rcpath, "scripts/config/%s", Cmd_Argv(1)); 
	FS_DefaultExtension(rcpath, ".rc" ); // append as default

	f = FS_LoadFile(rcpath, &len );
	if (!f)
	{
		MsgWarn("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}
	MsgDev(D_INFO, "execing %s\n",Cmd_Argv(1));
	Cbuf_InsertText(f);
	Z_Free (f);
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int	i;
	
	for(i = 1; i < Cmd_Argc(); i++)
		Msg ("%s ",Cmd_Argv(i));
	Msg ("\n");
}

/*
=====================================
Cmd_GetMapList

Prints or complete map filename
=====================================
*/
bool Cmd_GetMapList (const char *s, char *completedname, int length )
{
	search_t		*t;
	file_t		*f;
	char		message[MAX_QPATH];
	char		matchbuf[MAX_QPATH];
	byte		buf[MAX_SYSPATH]; // 1 kb
	int		i;

	t = FS_Search(va("maps/%s*.bsp", s), true );
	if( !t ) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	Msg("^3 %i maps found :\n", t->numfilenames);

	for(i = 0; i < t->numfilenames; i++)
	{
		const char	*data = NULL;
		char		*entities = NULL;
		char		entfilename[MAX_QPATH];
		int		ver = -1, lumpofs = 0, lumplen = 0;

		strncpy(message, "No Title", sizeof(message));
		f = FS_Open(t->filenames[i], "rb" );
	
		if( f )
		{
			memset(buf, 0, 1024);
			FS_Read(f, buf, 1024);
			if(!memcmp(buf, "IBSP", 4))
			{
				ver = LittleLong(((int *)buf)[1]);
				if(ver == BSPMOD_VERSION)
				{
					dheader_t *header = (dheader_t *)buf;
					lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
				}
			}

			std.strncpy(entfilename, t->filenames[i], sizeof(entfilename));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			entities = (char *)FS_LoadFile(entfilename, NULL);

			if( !entities && lumplen >= 10 )
			{
				FS_Seek(f, lumpofs, SEEK_SET);
				entities = (char *)Z_Malloc(lumplen + 1);
				FS_Read(f, entities, lumplen);
			}

			if( entities )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				message[0] = 0;
				data = entities;
				while(Com_ParseToken(&data))
				{
					if(!strcmp(com_token, "{" )) continue;
					else if(!strcmp(com_token, "}" )) break;
					else if(!strcmp(com_token, "message" ))
					{
						// get the message contents
						Com_ParseToken(&data);
						strncpy(message, com_token, sizeof(message));
					}
					else if(!strcmp(com_token, "mapversion" ))
					{
						// get map version
						Com_ParseToken(&data);
						ver = atoi(com_token);
					}
				}
			}
		}
		if( entities )Z_Free(entities);
		if( f )FS_Close(f);
		FS_FileBase(t->filenames[i], matchbuf );

		switch(ver)
		{
		case 38:  strncpy((char *)buf, "Q2", sizeof(buf)); break;
		case 220: strncpy((char *)buf, "Xash", sizeof(buf)); break;
		default:	strncpy((char *)buf, "??", sizeof(buf)); break;
		}
		Msg("%16s (%s) ^3%s^7\n", matchbuf, buf, message);
	}
	Msg("\n");
	Z_Free( t );

	// cut shortestMatch to the amount common with s
	for( i = 0; matchbuf[i]; i++ )
	{
		if(tolower(completedname[i]) != tolower(matchbuf[i]))
			completedname[i] = 0;
	}
	return true;
}

/*
=====================================
Cmd_GetDemoList

Prints or complete demo filename
=====================================
*/
bool Cmd_GetDemoList (const char *s, char *completedname, int length )
{
	search_t		*t;
	char		matchbuf[MAX_QPATH];
	int		i;

	t = FS_Search(va("demos/%s*.dm2", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	Msg("^3 %i demos found :\n", t->numfilenames);

	for(i = 0; i < t->numfilenames; i++)
	{
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
	}
	Msg("\n");
	Z_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(tolower(completedname[i]) != tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char			*name;
	char			*desc;
	xcommand_t		function;
} cmd_function_t;


static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char cmd_tokenized[MAX_INPUTLINE+MAX_STRING_TOKENS]; // will have 0 bytes inserted
static cmd_function_t *cmd_functions; // possible commands to execute

/*
============
Cmd_Argc
============
*/
int Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if((uint)arg >= cmd_argc )
		return "";
	return cmd_argv[arg];	
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char *Cmd_Args (void)
{
	static char cmd_args[MAX_STRING_CHARS];
	int	i;

	cmd_args[0] = 0;

	// build only for current call
	for ( i = 1; i < cmd_argc; i++ )
	{
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 )
			strcat( cmd_args, " " );
	}
	return cmd_args;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
void Cmd_TokenizeString (const char *text_in)
{
	const char	*text;
	char		*textOut;

	cmd_argc = 0; // clear previous args

	if(!text_in ) return;

	text = text_in;
	textOut = cmd_tokenized;

	while( 1 )
	{
		// this is usually something malicious
		if ( cmd_argc == MAX_STRING_TOKENS ) return;

		while ( 1 )
		{
			// skip whitespace
			while ( *text && *text <= ' ' ) text++;
			if ( !*text ) return; // all tokens parsed

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) return; // all tokens parsed

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' )
			{
				while(*text && ( text[0] != '*' || text[1] != '/' )) text++;
				if ( !*text ) return; // all tokens parsed
				text += 2;
			}
			else break; // we are ready to parse a token
		}

		// handle quoted strings
		if ( *text == '"' )
		{
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) *textOut++ = *text++;
			*textOut++ = 0;
			if ( !*text ) return; // all tokens parsed
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' )
		{
			if ( text[0] == '"' ) break;
			if ( text[0] == '/' && text[1] == '/' ) break;
			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) break;

			*textOut++ = *text++;
		}

		*textOut++ = 0;
		if( !*text ) return; // all tokens parsed
	}
	
}


/*
============
Cmd_AddCommand
============
*/
void _Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *cmd_desc)
{
	cmd_function_t	*cmd;
	
	// fail if the command already exists
	if(Cmd_Exists( cmd_name ))
	{
		MsgDev(D_INFO, "Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	// use a small malloc to avoid zone fragmentation
	cmd = Z_Malloc (sizeof(cmd_function_t));
	cmd->name = copystring( cmd_name );
	cmd->desc = copystring( cmd_desc );
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand (char *cmd_name)
{
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while( 1 )
	{
		cmd = *back;
		if (!cmd ) return;
		if (!strcmp( cmd_name, cmd->name ))
		{
			*back = cmd->next;
			if(cmd->name) Z_Free(cmd->name);
			if(cmd->desc) Z_Free(cmd->desc);
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
============
Cmd_CommandCompletion
============
*/
void Cmd_CommandCompletion( void(*callback)(const char *s, const char *m))
{
	cmd_function_t	*cmd;
	
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		callback( cmd->name, cmd->desc );
}

/*
============
Cmd_Exists
============
*/
bool Cmd_Exists (const char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcmp (cmd_name,cmd->name))
			return true;
	}
	return false;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void Cmd_ExecuteString( const char *text )
{	
	cmd_function_t	*cmd, **prev;

	// execute the command line
	Cmd_TokenizeString( text );		
	if( !Cmd_Argc()) return; // no tokens

	// check registered command functions	
	for ( prev = &cmd_functions; *prev; prev = &cmd->next )
	{
		cmd = *prev;
		if(!stricmp( cmd_argv[0], cmd->name ))
		{
			// rearrange the links so that the command will be
			// near the head of the list next time it is used
			*prev = cmd->next;
			cmd->next = cmd_functions;
			cmd_functions = cmd;

			// perform the action
			if(!cmd->function )
			{	// forward to server command
				Cmd_ExecuteString(va("cmd %s", text));
			}
			else cmd->function();
			return;
		}
	}
	
	// check cvars
	if(Cvar_Command()) return;

	// send it as a server command if we are connected
	Cmd_ForwardToServer();
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f (void)
{
	cmd_function_t	*cmd;
	int		i = 0;
	char		*match;

	if(Cmd_Argc() > 1) match = Cmd_Argv( 1 );
	else match = NULL;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (match && !Com_Filter(match, cmd->name, false))
			continue;
		Msg("%s\n", cmd->name);
		i++;
	}
	Msg("%i commands\n", i);
}

/*
============
Cmd_Init
============
*/
void Cmd_Init( int argc, char **argv )
{
	char	dev_level[4];

	Cbuf_Init( argc, argv );

	// register our commands
	Cmd_AddCommand ("exec", Cmd_Exec_f);
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand ("cmdlist", Cmd_List_f);
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f );

	// determine debug and developer mode
	if(FS_CheckParm ("-debug")) host.debug = true;
	if(FS_GetParmFromCmdLine("-dev", dev_level ))
		host.developer = atoi(dev_level);
}

