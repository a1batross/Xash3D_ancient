//=======================================================================
//			Copyright XashXT Group 2007 ©
//			con_main.c - client console
//=======================================================================

#include "common.h"
#include "client.h"

cvar_t	*con_notifytime;
cvar_t	*con_speed;

vec4_t console_color = {1.0, 1.0, 1.0, 1.0};
int g_console_field_width = 78;

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'

#define NUM_CON_TIMES	5		// need for 4 lines
#define CON_TEXTSIZE	MAX_MSGLEN * 4	// 128 kb buffer

// console color typeing
vec4_t g_color_table[8] =
{
{0.0, 0.0, 0.0, 1.0},
{1.0, 0.0, 0.0, 1.0},
{0.0, 1.0, 0.0, 1.0},
{1.0, 1.0, 0.0, 1.0},
{0.0, 0.0, 1.0, 1.0},
{0.0, 1.0, 1.0, 1.0},
{1.0, 0.0, 1.0, 1.0},
{1.0, 1.0, 1.0, 1.0},
};

typedef struct
{
	bool	initialized;

	short	text[CON_TEXTSIZE];
	int	current;		// line where next message will be printed
	int	x;		// offset in current line for next print
	int	display;		// bottom of console displays this line

	int 	linewidth;	// characters across screen
	int	totallines;	// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at con_speed
	float	finalFrac;	// 0.0 to 1.0 lines of console to display

	int	vislines;		// in scanlines
	dword	times[NUM_CON_TIMES]; // cls.realtime time the line was generated for transparent notify lines
	vec4_t	color;

} console_t;

static console_t con;

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	if(!host.developer) return;

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify();

	if( cls.key_dest == key_console )
	{
		UI_HideMenu();
	}
	else
	{
		UI_HideMenu();
		cls.key_dest = key_console;	

		if(com.atoi(cl.configstrings[CS_MAXCLIENTS]) == 1 && Host_ServerState())
			Cvar_SetValue( "paused", 1 );
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			UI_HideMenu();
			cls.key_dest = key_game;
		}
	}
	else cls.key_dest = key_console;
	
	Con_ClearNotify();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	int		i;

	for ( i = 0; i < CON_TEXTSIZE; i++ )
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	Con_Bottom(); // go to end
}
						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void )
{
	int		i;
	
	for( i = 0; i < NUM_CON_TIMES; i++ )
		con.times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int	i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

	width = ((int)Cvar_VariableValue("width")/ SMALLCHAR_WIDTH) - 2;

	if( width == con.linewidth )
		return;

	if( width < 1 ) // video hasn't been initialized yet
	{
		width = g_console_field_width;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i = 0; i < CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		Mem_Copy(tbuf, con.text, CON_TEXTSIZE * sizeof(short));
		for(i = 0; i < CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';

		for (i = 0; i < numlines; i++)
		{
			for (j = 0; j < numchars; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	int	i;

	Con_CheckResize();

	// register our commands
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0, "notify time to live" );
	con_speed = Cvar_Get ("con_speed", "3", 0, "console moving speed" );

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;
	for ( i = 0 ; i < COMMAND_HISTORY ; i++ )
	{
		Field_Clear( &historyEditLines[i] );
		historyEditLines[i].widthInChars = g_console_field_width;
	}

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f, "opens or closes the console" );
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f, "enable or disable chat mode" );
	Cmd_AddCommand ("messagemode", Con_MessageMode_f, "input a chat message to say to everyone" );
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f, "input a chat message to say to only your team" );
	Cmd_AddCommand ("clear", Con_Clear_f, "clear console history" );
	con.initialized = true;
	MsgDev( D_NOTE, "Console initialized.\n" );
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( bool skipnotify )
{
	int	i;

	// mark time for transparent overlay
	if (con.current >= 0)
	{
		if( skipnotify ) con.times[con.current % NUM_CON_TIMES] = 0;
		else con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}

	con.x = 0;
	if (con.display == con.current) con.display++;
	con.current++;
	for(i = 0; i < con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8)|' ';
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print( const char *txt )
{
	int	y, c, l, color;
	bool	skipnotify = false;
	int	prev;

	// client not running
	if(host.type == HOST_DEDICATED) return;
          if(!con.initialized) return;

	if(!com.strncmp( txt, "[skipnotify]", 12 ))
	{
		skipnotify = true;
		txt += 12;
	}
	
	color = ColorIndex(COLOR_WHITE);

	while((c = *txt) != 0 )
	{
		if(IsColorString( txt ))
		{
			color = ColorIndex(*(txt+1));
			txt += 2;
			continue;
		}

		// count word length
		for (l = 0; l < con.linewidth; l++)
		{
			if ( txt[l] <= ' ') break;
		}

		// word wrap
		if (l != con.linewidth && (con.x + l >= con.linewidth))
			Con_Linefeed( skipnotify);
		txt++;

		switch(c)
		{
		case '\n':
			Con_Linefeed( skipnotify );
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | c;
			con.x++;
			if( con.x >= con.linewidth )
			{
				Con_Linefeed( skipnotify );
				con.x = 0;
			}
			break;
		}
	}

	// mark time for transparent overlay
	if( con.current >= 0 )
	{
		if( skipnotify )
		{
			prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 ) prev = NUM_CON_TIMES - 1;
			con.times[prev] = 0;
		}
		else con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		y;

	if (cls.key_dest != key_console) return; // don't draw anything (always draw if not active)

	y = con.vislines - ( SMALLCHAR_HEIGHT * 2 );
	re->SetColor( con.color );
	Field_Draw( &g_consoleField, con.xadjust + 2 * SMALLCHAR_WIDTH, y, SCREEN_WIDTH - 3 * SMALLCHAR_WIDTH, true );
	SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, '>' );
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
	int		x, v = 0;
	short		*text;
	int		i;
	int		time;
	int		skip;
	int		currentColor;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	for( i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++ )
	{
		if( i < 0 ) continue;
		time = con.times[i % NUM_CON_TIMES];
		if( time == 0 ) continue;
		time = cls.realtime - time;
		if( time > con_notifytime->value * 1000 ) continue;
		text = con.text + (i % con.totallines) * con.linewidth;

		for( x = 0; x < con.linewidth; x++)
		{
			if((text[x] & 0xff ) == ' ' ) continue;
			if(((text[x]>>8)&7 ) != currentColor )
			{
				currentColor = (text[x]>>8)&7;
				re->SetColor(g_color_table[currentColor]);
			}
			SCR_DrawSmallChar( con.xadjust + (x+1)*SMALLCHAR_WIDTH, v, text[x] & 0xff );
		}
		v += SMALLCHAR_HEIGHT;
	}

	re->SetColor( NULL );

	// draw the chat line
	if( cls.key_dest == key_message )
	{
		if (chat_team)
		{
			SCR_DrawBigString (8, v, "say_team:", 1.0f );
			skip = 11;
		}
		else
		{
			SCR_DrawBigString (8, v, "say:", 1.0f );
			skip = 5;
		}
		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, v, SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, true );
		v += BIGCHAR_HEIGHT;
	}

}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole (float frac)
{
	int	i, x, y;
	int	rows;
	short	*text;
	int	row;
	int	lines;
	int	currentColor;
	vec4_t	color;
	string	curtime;

	lines = scr_height->integer * frac;
	if( lines <= 0 ) return;
	if( lines > scr_height->integer ) lines = scr_height->integer;

	con.xadjust = 0; // on wide screens, we will center the text
	SCR_AdjustSize( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT - 2;
	if ( y < 1 ) y = 0;
	else SCR_DrawPic( 0, y - SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, "background/conback" );

	Vector4Set( color, 1, 0, 0, 1 );
	SCR_FillRect( 0, y, SCREEN_WIDTH, 2, color );

	// draw current time
	re->SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
	com.snprintf( curtime, MAX_STRING, "%s ", timestamp( TIME_TIME_ONLY));
	i = com.strlen( curtime );
	for (x = 0; x < i; x++)
		SCR_DrawSmallChar(scr_width->integer - ( i - x ) * SMALLCHAR_WIDTH, (lines - (SMALLCHAR_HEIGHT+SMALLCHAR_HEIGHT/2)), curtime[x]);
	re->SetColor(NULL);

	// draw the text
	con.vislines = lines;
	rows = (lines - SMALLCHAR_WIDTH)/SMALLCHAR_WIDTH; // rows of text to draw
	y = lines - (SMALLCHAR_HEIGHT * 3);

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		re->SetColor(g_color_table[ColorIndex(COLOR_RED)]);
		for (x = 0; x < con.linewidth; x += 4)
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
		rows--;
	}
	
	row = con.display;
	if( con.x == 0 ) row--;

	currentColor = 7;
	re->SetColor( g_color_table[currentColor] );

	for (i = 0; i < rows; i++, y -= SMALLCHAR_HEIGHT, row--)
	{
		if (row < 0) break;
		if (con.current - row >= con.totallines)
		{
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines)*con.linewidth;

		for (x = 0; x < con.linewidth; x++)
		{
			if((text[x] & 0xff ) == ' ') continue;
			if(((text[x]>>8) & 7 ) != currentColor )
			{
				currentColor = (text[x]>>8) & 7;
				re->SetColor(g_color_table[currentColor]);
			}
			SCR_DrawSmallChar( con.xadjust + (x+1) * SMALLCHAR_WIDTH, y, text[x] & 0xff );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
	re->SetColor( NULL );
}

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void )
{
	if(!host.developer) return;

	// check for console width changes from a vid mode change
	Con_CheckResize ();

	// if disconnected, render console full screen
	switch( cls.state )
	{
	case ca_uninitialized:
		break;
	case ca_connected:
	case ca_connecting:
		if( host.developer )
		{
			// show console in devmode
			Con_DrawSolidConsole( 0.5f );
		}
		break;
	case ca_disconnected:
		if( cls.key_dest != key_menu )
		{
			Con_DrawSolidConsole( 1.0f );
			cls.key_dest = key_console;
		}
		break;
	case ca_active:
	case ca_cinematic: 
		if( con.displayFrac ) Con_DrawSolidConsole( con.displayFrac );
		else if ( cls.state == ca_active ) Con_DrawNotify(); // draw notify lines
		break;
	}
}

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void )
{
	if( !host.developer ) return;

	// decide on the destination height of the console
	if( cls.key_dest == key_console )
	{
		if ( cls.state == ca_disconnected )
			con.finalFrac = 1.0;// full screen
		else con.finalFrac = 0.5;	// half screen	
	}
	else con.finalFrac = 0; // none visible

	if (con.finalFrac < con.displayFrac)
	{
		con.displayFrac -= con_speed->value * cls.frametime;
		if (con.finalFrac > con.displayFrac)
			con.displayFrac = con.finalFrac;
	}
	else if( con.finalFrac > con.displayFrac )
	{
		con.displayFrac += con_speed->value * cls.frametime;
		if (con.finalFrac < con.displayFrac)
			con.displayFrac = con.finalFrac;
	}
}

void Con_PageUp( void )
{
	con.display -= 2;
	if( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

void Con_PageDown( void )
{
	con.display += 2;
	if (con.display > con.current) con.display = con.current;
}

void Con_Top( void )
{
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines )
		con.display = con.current - con.totallines + 1;
}

void Con_Bottom( void )
{
	con.display = con.current;
}

void Con_Close( void )
{
	Field_Clear( &g_consoleField );
	Con_ClearNotify();
	con.finalFrac = 0; // none visible
	con.displayFrac = 0;
	Cvar_SetValue( "paused", 0 );
}

bool Con_Active( void )
{
	return con.initialized;
}