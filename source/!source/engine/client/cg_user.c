//=======================================================================
//			Copyright XashXT Group 2007 ©
//		cg_user.c - stuff that will moved into client.dat
//=======================================================================

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

	SCR_DrawPic( scr_vrect.x+64, scr_vrect.y, 48, 48, "net" );
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

	CG_DrawCenterPic( 128, 32, "pause" );
}

/*
==============
SG_DrawLoading
==============
*/
void CG_DrawLoading( void )
{
	CG_DrawCenterPic( 128, 32, "loading" );
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

#define STAT_MINUS		10	// num frame for '-' stats digit
char		*sb_nums[2][11] = 
{
	{"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
	"num_6", "num_7", "num_8", "num_9", "num_minus"},
	{"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
	"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"}
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

		re->DrawPic (x,y,sb_nums[color][frame]);
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
	int		i, j;

	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<11 ; j++)
			re->RegisterPic (sb_nums[i][j]);

	if (crosshair->value)
	{
		if (crosshair->value > 3 || crosshair->value < 0)
			crosshair->value = 3;

		sprintf (crosshair_pic, "ch%i", (int)(crosshair->value));
		re->DrawGetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);
		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

/*
================
SCR_ExecuteLayoutString 

================
*/
void SCR_ExecuteLayoutString( char *section )
{
	int		x = 0, y = 0;
	int		value;
	int		width = 3;
	int		index;
	clientinfo_t	*ci;

	COM_ResetScript();

	if (cls.state != ca_active || !cl.refresh_prepped )
		return;

	while(COM_GetToken( true ))
	{
		// first, we must find start of section
		if(!strcmp(COM_Token, "void"))
		{
			// compare section with current
			COM_GetToken( false );
			if(!strcmp(COM_Token, section))			
			{
				// yes, it's
				Msg("compare found\n" );
				break;
			}
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
		}
		if (!strcmp(token, "if"))
		{	
			// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];

			if (!value) //find "}"
			{
				while (s && strcmp(token, "}") )
				{
					token = COM_Parse (&s);
				}
			}
			else // find "{"
			{
				while (s && strcmp(token, "{") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}*/
	}
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
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR]);
}


/*
================
SCR_DrawLayout

================
*/
#define	STAT_LAYOUTS		13

void SCR_DrawLayout (void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;
	SCR_ExecuteLayoutString (cl.layout);
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
	SCR_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, "splash" );
	//SCR_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT/3, "clouds" ); 
}

/*
================
V_RenderLogo

loading splash
================
*/
void V_RenderLogo( void )
{
	SCR_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cl.levelshot_name );
	CG_DrawLoading(); // loading.dds
}