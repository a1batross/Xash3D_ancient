//=======================================================================
//			Copyright XashXT Group 2007 ©
//		cg_user.c - stuff that will moved into client.dat
//=======================================================================

#include <ctype.h>
#include "client.h"

extern cvar_t	*scr_centertime;
extern cvar_t	*scr_showpause;
extern bool	scr_draw_loading;

#define FADE_TIME		0.5f

bool scr_draw_loading;

/*
=================
CG_SetSky_f

Set a specific sky and rotation speed
=================
*/
void CG_SetSky_f( void )
{
	float	rotate;
	vec3_t	axis;

	if(Cmd_Argc() < 2)
	{
		Msg("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}

	if(Cmd_Argc() > 2) rotate = atof(Cmd_Argv(2));
	else rotate = 0;
	if(Cmd_Argc() == 6)
	{
		VectorSet(axis, atof(Cmd_Argv(3)), atof(Cmd_Argv(4)), atof(Cmd_Argv(5)));
	}
	else
	{
		VectorSet(axis, 0, 0, 1 );
	}

	re->SetSky(Cmd_Argv(1), rotate, axis);
}

/*
==============================================================================

CG Draw Tools
==============================================================================
*/
/*
================
SCR_GetBigStringWidth

skips color escape codes
================
*/
int CG_GetBigStringWidth( const char *str )
{
	return ColorStrlen( str ) * 16;
}

/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( float starttime, float endtime )
{
	static vec4_t	color;
	float		time;

	if( starttime == 0 ) return NULL;
	time = cls.realtime - starttime;
	if( time >= endtime ) return NULL;

	// fade out
	if((endtime - time) < FADE_TIME )
		color[3] = (endtime - time) * 1.0f / FADE_TIME;
	else color[3] = 1.0;
	color[0] = color[1] = color[2] = 1.0f;

	return color;
}

int CG_StatsValue( const char *name )
{
	int	i, value = 0;

	for(i = 0; i < cls.cg_numaliases; i++)
	{
		if(!strcmp(cls.cg_alias[i].name, name ))
		{
			value = cls.cg_alias[i].value;
			break;
		}
	}

	if(i == cls.cg_numaliases)
	{
		MsgDev(D_WARN, "CG_StatsValue: can't use undefined alias %s\n", name );
		return 0;
	}

	return cl.frame.playerstate.stats[value];		
}

void CG_SetAlias( const char *name, int value )
{
	int	i;

	if(!name) return;
	if(strlen(name) > MAX_QPATH)
	{
		MsgDev(D_WARN, "CG_SetAlias: %s too long name, limit is %d\n", name, MAX_QPATH );
	}
	if(value < 0 || value >= MAX_STATS)
	{
		MsgDev(D_WARN, "CG_SetAlias: value %d out of range\n", value );
		value = bound(0, value, MAX_STATS - 1 );
	}
	if(cls.cg_numaliases + 1 >= MAX_STATS)
	{
		MsgDev(D_WARN, "CG_SetAlias: aliases limit exceeded\n" );
		return;
	}

	// check for duplicate
	for(i = 0; i < cls.cg_numaliases; i++)
	{
		if(!strcmp(cls.cg_alias[i].name, name ))
		{
			if(cls.cg_alias[i].value != value)
				MsgDev(D_WARN, "CG_SetAlias: redefinition alias %s\n", name );
			else MsgDev(D_WARN, "CG_SetAlias: duplicated alias %s\n", name );
			break;
		}		
	}

	// register new alias
	if(i == cls.cg_numaliases)
	{
		Msg("add alias %s = %d\n", name, value );
		strncpy(cls.cg_alias[cls.cg_numaliases].name, name, MAX_QPATH );
		cls.cg_alias[cls.cg_numaliases].value = value;
		cls.cg_numaliases++;
	}
}

void CG_SetColor( float r, float g, float b, float a )
{
	Vector4Set( cls.cg_color, r, g, b, a );
	re->SetColor( cls.cg_color );
}

void CG_ResetColor( void )
{
	Vector4Set( cls.cg_color, 1.0f, 1.0f, 1.0f, 1.0f );
	re->SetColor( cls.cg_color );
}

bool CG_ParseArgs( int num_argc )
{
	cls.cg_argc = 0;
	memset(cls.cg_argv, 0, MAX_PARMS * MAX_QPATH );
	strncpy( cls.cg_progname, COM_Token, MAX_QPATH );

	if( num_argc > MAX_PARMS )
	{
		MsgDev(D_WARN, "CG_ParseArgs: %s have too many args, limit is %d\n", cls.cg_progname, MAX_PARMS ); 
		num_argc = MAX_PARMS;
	}

	while(COM_TryToken())
	{
		if(cls.cg_argc > num_argc )
		{
			MsgWarn("CG_ParseArgs: syntax error in function %s\n", cls.cg_progname );
			return false; // stack overflow
		}
		else if(COM_MatchToken(";")) break;		// end of parsing
		else if(COM_MatchToken(",")) cls.cg_argc++;	// new argument
		else if(COM_MatchToken("(") || COM_MatchToken(")"))
			continue; // skip punctuation
		else strncpy(cls.cg_argv[cls.cg_argc], COM_Token, MAX_QPATH ); // fill stack
	}
	return true;
}

void CG_SkipBlock( void )
{
	int	old_depth =  cls.cg_program_depth;

	do {
		if(COM_MatchToken("{"))
		{
			// for bounds cheking
			cls.cg_program_depth++;
		}
		else if(COM_MatchToken("}")) 
		{
			cls.cg_program_depth--;
			break;
		}
		while(COM_TryToken());
	} while(COM_GetToken( true ));

	if(cls.cg_program_depth != old_depth)
	{
		MsgDev(D_ERROR, "CG_SkipBlock: missing } in %s\n", cls.cg_progname);
	}
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/
/*
===================
CG_DrawCenterString
===================
*/
void CG_DrawCenterString( void )
{
	char	*start;
	int	l, x, y, w;
	float	*color;

	if(!cl.centerPrintTime ) return;

	color = CG_FadeColor( cl.centerPrintTime, scr_centertime->value );
	if( !color ) 
	{
		cl.centerPrintTime = 0.0f;
		return;
	}

	re->SetColor( color );
	start = cl.centerPrint;
	y = cl.centerPrintY - cl.centerPrintLines * BIGCHAR_HEIGHT/2;

	while( 1 )
	{
		char linebuffer[1024];

		for ( l = 0; l < 50; l++ )
		{
			if ( !start[l] || start[l] == '\n' )
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cl.centerPrintCharWidth * ColorStrlen( linebuffer );
		x = ( SCREEN_WIDTH - w ) / 2;

		SCR_DrawStringExt( x, y, cl.centerPrintCharWidth, linebuffer, color, false );

		y += cl.centerPrintCharWidth * 1.5;
		while ( *start && ( *start != '\n' )) start++;
		if( !*start ) break;
		start++;
	}
	re->SetColor( NULL );
}

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth )
{
	char	*s;

	strncpy( cl.centerPrint, str, sizeof(cl.centerPrint));

	cl.centerPrintTime = cls.realtime;
	cl.centerPrintY = y;
	cl.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cl.centerPrintLines = 1;
	s = cl.centerPrint;
	while( *s )
	{
		if (*s == '\n') cl.centerPrintLines++;
		s++;
	}
}

/*
==============
CG_DrawCenterPic

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_DrawCenterPic( int w, int h, char *picname )
{
	SCR_DrawPic((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, picname );
}

/*
==============
SG_DrawNet
==============
*/
void CG_DrawNet( void )
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1)
		return;

	SCR_DrawPic( scr_vrect.x+64, scr_vrect.y, 48, 48, "hud/net" );
}

/*
==============
SG_DrawPause
==============
*/
void CG_DrawPause( void )
{
	// turn off for screenshots
	if(!cl_paused->value) return;
	if(!scr_showpause->value) return;

	CG_DrawCenterPic( 128, 32, "menu/m_pause" );
}

/*
==============
CG_MakeLevelShot

used as splash logo
==============
*/
void CG_MakeLevelShot( void )
{
	if(cl.make_levelshot)
	{
		Con_ClearNotify();
		cl.make_levelshot = false;

		// make levelshot at nextframe()
		Cbuf_AddText ("levelshot\n");
	}
}

void DrawString (int x, int y, char *s)
{
	while (*s)
	{
		SCR_DrawSmallChar(x, y, *s);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, char *s)
{
	while (*s)
	{
		SCR_DrawSmallChar(x, y, *s);
		x+=8;
		s++;
	}
}


//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;			// don't play ambients

	if (cls.disable_screen) return;
//if (cls.state == ca_disconnected) return;	// if at console, don't bring up the plaque
//if (cls.key_dest == key_console) return;
	if (cls.state == ca_cinematic) scr_draw_loading = 2;	// clear to black first
	else scr_draw_loading = 1;
 
	SCR_UpdateScreen ();
	cls.disable_screen = Sys_DoubleTime();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	return;

	cls.disable_screen = 0;
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

/*
================
SCR_TimeRefresh_f
================
*/
int entitycmpfnc( const entity_t *a, const entity_t *b )
{
	/*
	** all other models are sorted by model then skin
	*/
	if ( a->model == b->model )
	{
		return ( ( int ) a->image - ( int ) b->image );
	}
	else
	{
		return ( ( int ) a->model - ( int ) b->model );
	}
}

void SCR_TimeRefresh_f (void)
{
	int		i;
	float		start, stop;
	float		time;

	if ( cls.state != ca_active )
		return;

	start = Sys_DoubleTime();

	if (Cmd_Argc() == 2)
	{	// run without page flipping
		re->BeginFrame();
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;
			re->RenderFrame (&cl.refdef);
		}
		re->EndFrame();
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;

			re->BeginFrame();
			re->RenderFrame (&cl.refdef);
			re->EndFrame();
		}
	}

	stop = Sys_DoubleTime();
	time = stop - start;
	Msg ("%f seconds (%f fps)\n", time, 128/time);
}

#define STAT_MINUS		10 // num frame for '-' stats digit
char *cg_nums[11] =
{
"hud/num0",
"hud/num1",
"hud/num2",
"hud/num3",
"hud/num4",
"hud/num5",
"hud/num6",
"hud/num7",
"hud/num8",
"hud/num9",
"hud/num-",
};

#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8



/*
================
SizeHUDString

Allow embedded \n in the string
================
*/
void SizeHUDString (char *string, int *w, int *h)
{
	int		lines, width, current;

	lines = 1;
	width = 0;

	current = 0;
	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;
			if (current > width)
				width = current;
		}
		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}

void DrawHUDString (char *string, int x, int y, int centerwidth, int xor)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width*8)/2;
		else
			x = margin;
		for (i=0 ; i<width ; i++)
		{
			re->DrawChar (x, y, line[i]^xor);
			x += 8;
		}
		if (*string)
		{
			string++;	// skip the \n
			x = margin;
			y += 8;
		}
	}
}

void CG_DrawField( int value, int x, int y, int color )
{
	char	num[16], *ptr;
	int	l, frame;

	sprintf(num, "%i", value );
	l = strlen( num );
	ptr = num;

	re->SetColor( g_color_table[ColorIndex(color)] );

	while (*ptr && l)
	{
		if (*ptr == '-') frame = STAT_MINUS;
		else frame = *ptr -'0';
		SCR_DrawPic( x, y, GIANTCHAR_WIDTH, GIANTCHAR_HEIGHT, cg_nums[frame]);
		x += GIANTCHAR_WIDTH;
		ptr++;
		l--;
	}

	re->SetColor( NULL );
}

/*
==============
SCR_DrawField
==============
*/
void SCR_DrawField (int x, int y, int color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	sprintf (num, "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		re->DrawPic (x,y,cg_nums[frame]);
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}


/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	if (crosshair->value)
	{
		if (crosshair->value > 3 || crosshair->value < 0)
			crosshair->value = 3;

		sprintf (crosshair_pic, "hud/ch%i", (int)(crosshair->value));
		re->DrawGetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);
		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

/*
================
CG_ExecuteProgram

================
*/
void CG_ExecuteProgram( char *section )
{
	bool		skip = true;

	COM_ResetScript();
	cls.cg_program_depth = 0;

	while(COM_GetToken( true ))
	{
		// first, we must find start of section
		if(COM_MatchToken("void"))
		{
			COM_GetToken( false ); // get section name
			if(COM_MatchToken( section )) skip = false;
			CG_ParseArgs( 1 ); // go to end of line
		}

		if(skip) continue;
		//Msg("execute %s\n", section );

		// exec hud program
		if(COM_MatchToken("{"))
		{
			cls.cg_program_depth++;
			continue;
		}
		else if(COM_MatchToken("}"))
		{
			cls.cg_program_depth--;
			if(!cls.cg_program_depth) break; // end of section
			continue;
		}

		// builtins
		if(COM_MatchToken("if"))
		{	
			bool	equal = true;
			int	value = 0;

			if(!CG_ParseArgs( 1 )) continue;
			if( cls.cg_argv[0][0] == '!' ) equal = false;
			value = CG_StatsValue(cls.cg_argv[0] + (equal ? 0 : 1));

			if(value && equal) continue;
			else if(!value && !equal) continue;
			else CG_SkipBlock(); // skip if{ }
		}
		else if(COM_MatchToken("SetAlias"))
		{
			// set alias name for stats numbers
			if(!CG_ParseArgs( 2 )) continue;
			CG_SetAlias(cls.cg_argv[0], atoi(cls.cg_argv[1]));
		}
		else if(COM_MatchToken("LoadPic"))
		{
			// cache hud pics
			if(!CG_ParseArgs( 1 )) continue;
			re->RegisterPic(cls.cg_argv[0]);
		}
		else if(COM_MatchToken("DrawField"))
		{
			// displayed health, armor, e.t.c.
			if(!CG_ParseArgs( 4 )) continue;
			CG_DrawField(CG_StatsValue(cls.cg_argv[0]), atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]));
		}
		else if(COM_MatchToken("DrawPic"))
		{
			// draw named pic
			if(!CG_ParseArgs( 3 )) continue;
			SCR_DrawPic( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), 48, 48, cls.cg_argv[0]);
		}
		else if(COM_MatchToken("DrawStretchPic"))
		{
			// draw named pic
			if(!CG_ParseArgs( 5 )) continue;
			SCR_DrawPic( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]), atoi(cls.cg_argv[4]), cls.cg_argv[0]);
		}
		else if(COM_MatchToken("SetColor"))
		{
			// set custom color
			if(!CG_ParseArgs( 4 )) continue;
			CG_SetColor(atof(cls.cg_argv[0]), atof(cls.cg_argv[1]), atof(cls.cg_argv[2]), atof(cls.cg_argv[3])); 
		}
		else if(COM_MatchToken("DrawCenterPic"))
		{
			// draw named pic
			if(!CG_ParseArgs( 3 )) continue;
			CG_DrawCenterPic( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), cls.cg_argv[0]);
		}
		else if(COM_MatchToken("DrawLevelShot"))
		{
			// draw named pic
			if(!CG_ParseArgs( 0 )) continue;
			SCR_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cl.levelshot_name );
		}
		/*if(!strcmp(token, "x"))
		{
			token = COM_Parse (&s);
			x = atoi(token);
			continue;
		}
		if (!strcmp(token, "y"))
		{
			token = COM_Parse (&s);
			y = atoi(token);
			continue;
		}		
		if (!strcmp(token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi(token);
			continue;
		}
		if (!strcmp(token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.width + atoi(token);
			continue;
		}
		if (!strcmp(token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			continue;
		}
		if (!strcmp(token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi(token);
			continue;
		}
		if (!strcmp(token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.height + atoi(token);
			continue;
		}
		if (!strcmp(token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			continue;
		}
		if (!strcmp(token, "pic"))
		{
			// draw a pic from a stat number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (value >= MAX_IMAGES) continue;
			SCR_DrawPic( x, y, 48, 48, cl.configstrings[CS_IMAGES+value] );
			continue;
		}
		if (!strcmp(token, "client"))
		{
			// draw a deathmatch client block
			int		score, ping, time;

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Host_Error("client >= MAX_CLIENTS\n");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);

			token = COM_Parse (&s);
			time = atoi(token);

			DrawAltString (x+32, y, ci->name);
			DrawString (x+32, y+8,  "Score: ");
			DrawAltString (x+32+7*8, y+8,  va("%i", score));
			DrawString (x+32, y+16, va("Ping:  %i", ping));
			DrawString (x+32, y+24, va("Time:  %i", time));

			if (!ci->icon)
				ci = &cl.baseclientinfo;
			re->DrawPic (x, y, ci->iconname);
			continue;
		}
		if (!strcmp(token, "ctf"))
		{
			// draw a ctf client block
			int	score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Host_Error("client >= MAX_CLIENTS\n");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);
			if (ping > 999) ping = 999;

			sprintf(block, "%3d %3d %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
				DrawAltString (x, y, block);
			else DrawString (x, y, block);
			continue;
		}
		if (!strcmp(token, "picn"))
		{
			// draw a pic from a name
			token = COM_Parse (&s);
			re->DrawPic (x, y, token);
			continue;
		}
		if (!strcmp(token, "num"))
		{
			// draw a number
			token = COM_Parse (&s);
			width = atoi(token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			SCR_DrawField (x, y, 0, width, value);
			continue;
		}
		if (!strcmp(token, "hnum"))
		{
			// health number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];
			if (value > 25) color = 0;	// green
			else if (value > 0) color = (cl.frame.serverframe>>2) & 1; // flash
			else color = 1;

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
				re->DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}
		if (!strcmp(token, "anum"))
		{
			// ammo number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];
			if (value > 5) color = 0;	// green
			else if (value >= 0) color = (cl.frame.serverframe>>2) & 1;	// flash
			else continue; // negative number = don't show

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
				re->DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}
		if (!strcmp(token, "rnum"))
		{
			// armor number
			int	color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value < 1) continue;

			color = 0; // green

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
				re->DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}
		if (!strcmp(token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi(token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Host_Error("Bad stat_string index\n");
			index = cl.frame.playerstate.stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Host_Error("Bad stat_string index\n");
			DrawString (x, y, cl.configstrings[index]);
			continue;
		}
		if (!strcmp(token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, 0);
			continue;
		}
		if (!strcmp(token, "string"))
		{
			token = COM_Parse (&s);
			DrawString (x, y, token);
			continue;
		}
		if (!strcmp(token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320,0x80);
			continue;
		}
		if (!strcmp(token, "string2"))
		{
			token = COM_Parse (&s);
			DrawAltString (x, y, token);
			continue;
		}*/
	}

	CG_ResetColor(); // don't forget reset color
}


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
	if (cls.state != ca_active || !cl.refresh_prepped )
		return;

	CG_ExecuteProgram(cl.configstrings[CS_STATUSBAR]);
}


/*
================
SCR_DrawLayout

================
*/
#define	STAT_LAYOUTS		13

void SCR_DrawLayout (void)
{
	if (cls.state != ca_active || !cl.refresh_prepped )
		return;

	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;
	CG_ExecuteProgram(cl.layout);
}


/*
================
V_RenderHUD

user hud rendering
================
*/
void V_RenderHUD( void )
{
	CG_MakeLevelShot();
	CG_DrawCenterString();
	CG_DrawPause();
	CG_DrawNet();

	// move into client.dat
	SCR_DrawStats();
	if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 1) SCR_DrawLayout();
	if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 2) CL_DrawInventory();
}

/*
================
V_RenderSplash

menu background
================
*/
void V_RenderSplash( void )
{
	CG_ExecuteProgram( "Hud_MenuBackground" );
}

/*
================
V_RenderLogo

loading splash
================
*/
void V_RenderLogo( void )
{
	CG_ExecuteProgram( "Hud_DrawPlaque" );
}

/*
================
CG_Init

initialize cg
================
*/
void CG_Init( void )
{
	cls.cg_numaliases = 0; // reset hudprogram aliasnames
	COM_LoadScript( "scripts/hud.txt", NULL, 0 );
	CG_ExecuteProgram( "Hud_Setup" ); // get alias names
}