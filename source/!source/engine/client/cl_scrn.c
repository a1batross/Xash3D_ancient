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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

  full screen console
  put up loading plaque
  blanked background with loading plaque
  blanked background with menu
  cinematics
  full screen image for quit and victory

  end of unit intermissions

  */

#include "client.h"

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

bool	scr_initialized;		// ready to draw

int			scr_draw_loading;

vrect_t		scr_vrect;		// position of render window on screen


cvar_t		*scr_viewsize;
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

typedef struct
{
	int		x1, y1, x2, y2;
} dirty_t;

dirty_t		scr_dirty, scr_old_dirty[2];

char		crosshair_pic[MAX_QPATH];
int			crosshair_width, crosshair_height;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	char	*s;
	char	line[64];
	int		i, j, l;

	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;
	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	// echo it to the console
	Msg("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = str;
	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (s[l] == '\n' || !s[l])
				break;
		for (i=0 ; i<(40-l)/2 ; i++)
			line[i] = ' ';

		for (j=0 ; j<l ; j++)
		{
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i+1] = 0;

		Msg ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;		// skip the \n
	} while (1);
	Msg("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = viddef.height*0.35;
	else
		y = 48;

	do	
	{
		// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (viddef.width - l*8)/2;
		SCR_AddDirtyPoint (x, y);
		for (j=0 ; j<l ; j++, x+=8)
		{
			re->DrawChar (x, y, start[j]);	
			if (!remaining--)
				return;
		}
		SCR_AddDirtyPoint (x, y+8);
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.frametime;
	
	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
=================
SCR_CalcVrect

Sets scr_vrect, the coordinates of the rendered window
=================
*/
static void SCR_CalcVrect (void)
{
	int		size;

	// bound viewsize
	if (scr_viewsize->value < 40)
		Cvar_Set ("viewsize","40");
	if (scr_viewsize->value > 100)
		Cvar_Set ("viewsize","100");

	size = scr_viewsize->value;

	scr_vrect.width = viddef.width*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = viddef.height*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (viddef.width - scr_vrect.width)/2;
	scr_vrect.y = (viddef.height - scr_vrect.height)/2;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize->value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize->value-10);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed
=================
*/
void SCR_Sky_f (void)
{
	float	rotate;
	vec3_t	axis;

	if (Cmd_Argc() < 2)
	{
		Msg ("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}
	if (Cmd_Argc() > 2)
		rotate = atof(Cmd_Argv(2));
	else
		rotate = 0;
	if (Cmd_Argc() == 6)
	{
		axis[0] = atof(Cmd_Argv(3));
		axis[1] = atof(Cmd_Argv(4));
		axis[2] = atof(Cmd_Argv(5));
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	re->SetSky (Cmd_Argv(1), rotate, axis);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);

	// register our commands
	Cmd_AddCommand ("timerefresh",SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading",SCR_Loading_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
	Cmd_AddCommand ("skyname",SCR_Sky_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged 
		< CMD_BACKUP-1)
		return;

	re->DrawPic (scr_vrect.x+64, scr_vrect.y, "net");
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawString (char *str)
{
	char string[128];
	int length;
	int xoffset;

	strcpy(string, str);
          length = strlen(string);
	xoffset = (length * 8)/2;
          
	re->DrawString((viddef.width)/2 - xoffset, viddef.height/2 + 8, string ); 
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	if (!scr_showpause->value)		// turn off for screenshots
		return;

	if (!cl_paused->value)
		return;

	SCR_DrawString( "pause" );
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;
	SCR_DrawString( "loading" );
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

	/*if (cls.disable_screen) return;
	if (host.developer) return;
	if (cls.state == ca_disconnected) return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console) return;
	if (cls.state == ca_cinematic) scr_draw_loading = 2;	// clear to black first
	else scr_draw_loading = 1;
          */
 
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

/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustSize( float *x, float *y, float *w, float *h )
{
	float	xscale;
	float	yscale;

	// scale for screen sizes
	xscale = viddef.width / SCREEN_WIDTH;
	yscale = viddef.height / SCREEN_HEIGHT;

	if(x) *x *= xscale;
	if(y) *y *= yscale;
	if(w) *w *= xscale;
	if(h) *h *= yscale;
}

/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
================
*/
void SCR_DrawPic( float x, float y, float width, float height, char *picname )
{
	SCR_AdjustSize( &x, &y, &width, &height );
	re->DrawStretchPic (x, y, width, height, 0, 0, 1, 1, picname );
}

/*
================
SCR_DrawChar

chars are drawn at 640*480 virtual screen size
================
*/
static void SCR_DrawChar( int x, int y, float size, int ch )
{
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if( ch == ' ' )return;
	if(y < -size) return;

	ax = x;
	ay = y;
	aw = size;
	ah = size;
	SCR_AdjustSize( &ax, &ay, &aw, &ah );

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re->DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size, "conchars" );
}

/*
====================
SCR_DrawSmallChar

small chars are drawn at native screen resolution
====================
*/
void SCR_DrawSmallChar( int x, int y, int ch )
{
	int	row, col;
	float	frow, fcol;
	float	size;


	ch &= 255;

	if( ch == ' ' )return;
	if(y < -SMALLCHAR_HEIGHT) return;

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re->DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, fcol, frow, fcol + size, frow + size, "conchars" );
}

/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, bool forceColor )
{
	vec4_t		color;
	const char	*s;
	int		xx;

	// draw the colored text
	s = string;
	xx = x;

	re->SetColor( setColor );
	while ( *s )
	{
		if(IsColorString( s ))
		{
			if( !forceColor )
			{
				Mem_Copy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ));
				color[3] = setColor[3];
				re->SetColor( color );
			}
			s += 2;
			continue;
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += SMALLCHAR_WIDTH;
		s++;
	}
	re->SetColor( NULL );
}

/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, float *setColor, bool forceColor )
{
	vec4_t		color;
	const char	*s;
	int		xx;

	// draw the drop shadow
	Vector4Set( color, 0.0f, 0.0f, 0.0f, setColor[3] );
	re->SetColor( color );
	s = string;
	xx = x;

	while ( *s )
	{
		if(IsColorString( s ))
		{
			s += 2;
			continue;
		}
		SCR_DrawChar( xx + 2, y + 2, size, *s );
		xx += size;
		s++;
	}

	// draw the colored text
	s = string;
	xx = x;
	re->SetColor( setColor );
	while ( *s )
	{
		if(IsColorString( s ))
		{
			if ( !forceColor )
			{
				Mem_Copy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ));
				color[3] = setColor[3];
				re->SetColor( color );
			}
			s += 2;
			continue;
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size;
		s++;
	}
	re->SetColor( NULL );
}


void SCR_DrawBigString( int x, int y, const char *s, float alpha )
{
	float	color[4];

	Vector4Set( color, 1.0f, 1.0f, 1.0f, alpha );
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, false );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color )
{
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, true );
}


/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color )
{
	re->SetColor( color );
	SCR_AdjustSize( &x, &y, &width, &height );
	re->DrawFill( x, y, width, height );
	re->SetColor( NULL );
}

/*
=================
SCR_AddDirtyPoint
=================
*/
void SCR_AddDirtyPoint (int x, int y)
{
	if (x < scr_dirty.x1)
		scr_dirty.x1 = x;
	if (x > scr_dirty.x2)
		scr_dirty.x2 = x;
	if (y < scr_dirty.y1)
		scr_dirty.y1 = y;
	if (y > scr_dirty.y2)
		scr_dirty.y2 = y;
}

void SCR_DirtyScreen (void)
{
	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear (void)
{
	int		i;
	int		top, bottom, left, right;
	dirty_t	clear;

	if (scr_con_current == 1.0)
		return;		// full screen console
	if (scr_viewsize->value == 100)
		return;		// full screen rendering
	if(cls.state == ca_cinematic) return; // full screen cinematic

	// erase rect will be the union of the past three frames
	// so tripple buffering works properly
	clear = scr_dirty;
	for (i=0 ; i<2 ; i++)
	{
		if (scr_old_dirty[i].x1 < clear.x1)
			clear.x1 = scr_old_dirty[i].x1;
		if (scr_old_dirty[i].x2 > clear.x2)
			clear.x2 = scr_old_dirty[i].x2;
		if (scr_old_dirty[i].y1 < clear.y1)
			clear.y1 = scr_old_dirty[i].y1;
		if (scr_old_dirty[i].y2 > clear.y2)
			clear.y2 = scr_old_dirty[i].y2;
	}

	scr_old_dirty[1] = scr_old_dirty[0];
	scr_old_dirty[0] = scr_dirty;

	scr_dirty.x1 = 9999;
	scr_dirty.x2 = -9999;
	scr_dirty.y1 = 9999;
	scr_dirty.y2 = -9999;

	// don't bother with anything convered by the console)
	top = scr_con_current*viddef.height;
	if (top >= clear.y1)
		clear.y1 = top;

	if (clear.y2 <= clear.y1)
		return;		// nothing disturbed

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	if (clear.y1 < top)
	{	// clear above view screen
		i = clear.y2 < top-1 ? clear.y2 : top-1;
		re->DrawTileClear (clear.x1 , clear.y1,
			clear.x2 - clear.x1 + 1, i - clear.y1+1, "backtile");
		clear.y1 = top;
	}
	if (clear.y2 > bottom)
	{	// clear below view screen
		i = clear.y1 > bottom+1 ? clear.y1 : bottom+1;
		re->DrawTileClear (clear.x1, i,
			clear.x2-clear.x1+1, clear.y2-i+1, "backtile");
		clear.y2 = bottom;
	}
	if (clear.x1 < left)
	{	// clear left of view screen
		i = clear.x2 < left-1 ? clear.x2 : left-1;
		re->DrawTileClear (clear.x1, clear.y1,
			i-clear.x1+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x1 = left;
	}
	if (clear.x2 > right)
	{	// clear left of view screen
		i = clear.x1 > right+1 ? clear.x1 : right+1;
		re->DrawTileClear (i, clear.y1,
			clear.x2-i+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x2 = right;
	}

}


//===============================================================


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

	SCR_AddDirtyPoint (x, y);
	SCR_AddDirtyPoint (x+width*CHAR_WIDTH+2, y+23);

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
void SCR_ExecuteLayoutString (char *s)
{
	int		x, y;
	int		value;
	char		*token;
	int		width;
	int		index;
	clientinfo_t	*ci;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0]) return;

	x = 0;
	y = 0;
	width = 3;

	while (s)
	{
		token = COM_Parse (&s);
		if(!token) break;
		
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
			if (value >= MAX_IMAGES) Host_Error("Pic >= MAX_IMAGES\n");
			if (cl.configstrings[CS_IMAGES+value])
			{
				SCR_AddDirtyPoint (x, y);
				SCR_AddDirtyPoint (x+23, y+23);
				re->DrawPic (x, y, cl.configstrings[CS_IMAGES+value]);
			}
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
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

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
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

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
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+23, y+23);
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
		}
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

//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( void )
{
	re->BeginFrame();

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( cls.state != ca_active )
	{
		if( viddef.width * 480 > viddef.height * 640 )
		{
			re->SetColor( g_color_table[0] );
			re->DrawStretchPic( 0, 0, viddef.width, viddef.height, 0, 0, 1, 1, "backtile" );
			re->SetColor( NULL );
		}
	}

	switch( cls.state )
	{
	case ca_cinematic:
		SCR_DrawCinematic();
		break;
	case ca_disconnected:
		M_Draw();
		break;
	case ca_connecting:
	case ca_connected:
		SCR_TileClear();
		SCR_DrawLoading();
		break;
	case ca_active:
		SCR_CalcVrect();
		V_RenderView();
		SCR_DrawStats();
		if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 1) SCR_DrawLayout();
		if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 2) CL_DrawInventory();
		break;
	case ca_uninitialized:
		return;
	default:
		Host_Error("SCR_DrawScreenField: bad cls.state" );
		break;
	}

	SCR_DrawNet();
	SCR_CheckDrawCenterString();
	SCR_DrawPause();
         
	SCR_DrawCinematic();
	M_Draw();
	Con_DrawConsole();
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	if (!scr_initialized)
		return;

	SCR_DrawScreenField();
	re->EndFrame();
}
