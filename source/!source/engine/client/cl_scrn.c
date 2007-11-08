//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_scrn.c - build client frame
//=======================================================================

#include "client.h"

bool	scr_initialized;		// ready to draw
vrect_t	scr_vrect;		// position of render window on screen


cvar_t		*scr_viewsize;
cvar_t		*scr_centertime;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

char	crosshair_pic[MAX_QPATH];
int	crosshair_width, crosshair_height;

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
	xscale = viddef.width / SCREEN_WIDTH;
	yscale = viddef.height / SCREEN_HEIGHT;

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
	// to avoid drawing r_notexture image
	if(!picname || !*picname ) return;

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
		Host_Error("SCR_DrawScreenField: bad cls.state" );
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
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);

	// register our commands
	Cmd_AddCommand ("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading", SCR_Loading_f);
	Cmd_AddCommand ("skyname", CG_SetSky_f );

	scr_initialized = true;

}