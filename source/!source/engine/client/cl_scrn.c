//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_scrn.c - build client frame
//=======================================================================

#include "client.h"

bool	scr_initialized;		// ready to draw
vrect_t	scr_vrect;		// position of render window on screen

cvar_t *scr_viewsize;
cvar_t *scr_centertime;
cvar_t *scr_showpause;
cvar_t *scr_printspeed;
cvar_t *scr_loading;
cvar_t *cl_levelshot_name;

void SCR_TimeRefresh_f( void );
void SCR_Loading_f( void );

/*
================
SCR_AdjustSize

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustSize( float *x, float *y, float *w, float *h )
{
	float	xscale;
	float	yscale;

	// scale for screen sizes
	xscale = viddef.width / 640.0f;
	yscale = viddef.height / 480.0f;

	if(x) *x *= xscale;
	if(y) *y *= yscale;
	if(w) *w *= xscale;
	if(h) *h *= yscale;
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
================
SCR_DrawPic

Coordinates are 640*480 virtual values
================
*/
void SCR_DrawPic( float x, float y, float width, float height, char *picname )
{
	int	w, h;

	// to avoid drawing r_notexture image
	if(!picname || !*picname ) return;

	// get original size
	if(width == -1 || height == -1)
	{
		re->DrawGetPicSize( &w, &h, picname );
		width = w, height = h;
	}
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

	re->DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size, "fonts/conchars" );
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

	re->DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, fcol, frow, fcol + size, frow + size, "fonts/conchars" );
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
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet( void )
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1)
		return;

	SCR_DrawPic( scr_vrect.x+64, scr_vrect.y, 48, 48, "hud/net" );
}

void SCR_DrawFPS( void )
{
	float		calc;
	static double	nexttime = 0, lasttime = 0;
	static double	framerate = 0;
	static int	framecount = 0;
	double		newtime;
	bool		red = false; // fps too low
	char		fpsstring[32];
	float		*color;

	if(cls.state != ca_active) return; 
	
	newtime = Sys_DoubleTime();
	if (newtime >= nexttime)
	{
		framerate = framecount / (newtime - lasttime);
		lasttime = newtime;
		nexttime = max(nexttime + 1, lasttime - 1);
		framecount = 0;
	}
	framecount++;
	calc = framerate;

	if ((red = (calc < 1.0f)))
	{
		std.snprintf(fpsstring, sizeof(fpsstring), "%4i spf", (int)(1.0f / calc + 0.5));
		color = g_color_table[1];
	}
	else
	{
		std.snprintf(fpsstring, sizeof(fpsstring), "%4i fps", (int)(calc + 0.5));
		color = g_color_table[3];
          }
	SCR_DrawBigStringColor(SCREEN_WIDTH - 146, SCREEN_HEIGHT - 32, fpsstring, color );
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void )
{
	if(!V_PreRender()) return;

	switch( cls.state )
	{
	case ca_cinematic:
		SCR_DrawCinematic();
		break;
	case ca_disconnected:
		V_RenderSplash();
		break;
	case ca_connecting:
	case ca_connected:
		V_RenderLogo();
		break;
	case ca_active:
		V_CalcRect();
		V_RenderView();
		V_RenderHUD();
		break;
	default:
		Host_Error("SCR_UpdateScreen: bad cls.state" );
		break;
	}

	V_PostRender();
}

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_showpause = Cvar_Get("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get("scr_printspeed", "8", 0);
	cl_levelshot_name = Cvar_Get("cl_levelshot_name", "common/black", 0 );

	// register our commands
	Cmd_AddCommand ("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading", SCR_Loading_f);
	Cmd_AddCommand ("skyname", CL_SetSky_f );

	scr_initialized = true;

}