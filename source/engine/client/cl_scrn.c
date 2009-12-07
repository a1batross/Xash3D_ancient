//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_scrn.c - build client frame
//=======================================================================

#include "common.h"
#include "client.h"

cvar_t *scr_viewsize;
cvar_t *scr_centertime;
cvar_t *scr_printspeed;
cvar_t *scr_loading;
cvar_t *scr_download;
cvar_t *scr_width;
cvar_t *scr_height;
cvar_t *cl_testentities;
cvar_t *cl_testlights;
cvar_t *cl_testflashlight;
cvar_t *cl_levelshot_name;
cvar_t *cl_envshot_size;
cvar_t *cl_font;
static bool scr_init = false;

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
	float	xscale, yscale;

	// scale for screen sizes
	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const rgba_t color )
{
	re->SetColor( color );
	SCR_AdjustSize( &x, &y, &width, &height );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.fillShader );
	re->SetColor( NULL );
}

/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
================
*/
void SCR_DrawPic( float x, float y, float width, float height, string_t shader )
{
	int	w, h;

	// get original size
	if( width == -1 || height == -1 )
	{
		re->GetParms( &w, &h, NULL, 0, shader );
		width = w, height = h;
	}
	SCR_AdjustSize( &x, &y, &width, &height );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, shader );
}

/*
================
SCR_DrawChar

chars are drawn at 640*480 virtual screen size
================
*/
void SCR_DrawChar( int x, int y, float w, float h, int ch )
{
	float	size, frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if( ch == ' ' ) return;
	if(y < -h) return;

	ax = x;
	ay = y;
	aw = w;
	ah = h;
	SCR_AdjustSize( &ax, &ay, &aw, &ah );
	
	frow = (ch >> 4)*0.0625f + (0.5f / 256.0f);
	fcol = (ch & 15)*0.0625f + (0.5f / 256.0f);
	size = 0.0625f - (1.0f / 256.0f);

	re->DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size, cls.clientFont );
}

/*
====================
SCR_DrawSmallChar

small chars are drawn at native screen resolution
console only
====================
*/
void SCR_DrawSmallChar( int x, int y, int ch )
{
	float	frow, fcol;
	float	size;

	ch &= 255;

	if( ch == ' ' ) return;
	if( y < -SMALLCHAR_HEIGHT ) return;

	frow = (ch >> 4)*0.0625f + (0.5f / 256.0f);
	fcol = (ch & 15)*0.0625f + (0.5f / 256.0f);
	size = 0.0625f - (1.0f / 256.0f);

	re->DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, fcol, frow, fcol + size, frow + size, cls.consoleFont );
}

/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float w, float h, const char *string, rgba_t setColor, bool forceColor )
{
	rgba_t		color;
	const char	*s;
	int		xx;

	// draw the drop shadow
	MakeRGBA( color, 0, 0, 0, setColor[3] );
	re->SetColor( color );
	s = string;
	xx = x;

	while( *s )
	{
		if( IsColorString( s ))
		{
			s += 2;
			continue;
		}
		SCR_DrawChar( xx + 2, y + 2, w, h, *s );
		xx += w;
		s++;
	}

	// draw the colored text
	s = string;
	xx = x;
	re->SetColor( setColor );
	while( *s )
	{
		if( IsColorString( s ))
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
		SCR_DrawChar( xx, y, w, h, *s );
		xx += w;
		s++;
	}
	re->SetColor( NULL );
}

void SCR_DrawBigString( int x, int y, const char *s, byte alpha )
{
	rgba_t	color;

	MakeRGBA( color, 255, 255, 255, alpha );
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, s, color, false );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, rgba_t color )
{
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, s, color, true );
}

/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, rgba_t setColor, bool forceColor )
{
	rgba_t		color;
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
	if( cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1 )
		return;

	SCR_DrawPic( cl.refdef.viewport[0] + 64, cl.refdef.viewport[1], 48, 48, cls.netIcon );
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS( void )
{
	float		calc;
	static double	nexttime = 0, lasttime = 0;
	static double	framerate = 0;
	static int	framecount = 0;
	double		newtime;
	bool		red = false; // fps too low
	char		fpsstring[32];
	byte		*color;

	if( cls.state != ca_active ) return; 
	if( !cl_showfps->integer ) return;
	if( cls.scrshot_action != scrshot_inactive )
		return;
	
	newtime = Sys_DoubleTime();
	if( newtime >= nexttime )
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
		com.snprintf( fpsstring, sizeof( fpsstring ), "%4i spf", (int)(1.0f / calc + 0.5));
		color = g_color_table[1];
	}
	else
	{
		com.snprintf( fpsstring, sizeof( fpsstring ), "%4i fps", (int)(calc + 0.5));
		color = g_color_table[7];
          }

	SCR_DrawStringExt( SCREEN_WIDTH - 68, 4, 8, 12, fpsstring, color, true );
}

/*
================
SCR_RSpeeds
================
*/
void SCR_RSpeeds( void )
{
	char	msg[MAX_SYSPATH];

	if( re->RSpeedsMessage( msg, sizeof( msg )))
	{
		int	x, y, height;
		char	*p, *start, *end;
		rgba_t	color;

		x = SCREEN_WIDTH - 320;
		y = 64;
		height = SMALLCHAR_HEIGHT;
		MakeRGBA( color, 255, 255, 255, 255 );

		p = start = msg;
		do
		{
			end = com.strchr( p, '\n' );
			if( end ) msg[end-start] = '\0';

			SCR_DrawStringExt( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, p, color, true );
			y += height;

			if( end ) p = end + 1;
			else break;
		} while( 1 );
	}
}

void SCR_MakeLevelShot( void )
{
	if( cls.scrshot_request != scrshot_plaque )
		return;

	// make levelshot at nextframe()
	Cbuf_AddText( "wait 1\nlevelshot\n" );
}

void SCR_MakeScreenShot( void )
{
	if( !re && host.type == HOST_NORMAL )
		return;	// don't reset action - it will be wait for render initalization is done

	switch( cls.scrshot_action )
	{
	case scrshot_plaque:
		if( re ) re->ScrShot( cls.shotname, VID_LEVELSHOT );
		break;
	case scrshot_savegame:
		if( re ) re->ScrShot( cls.shotname, VID_SAVESHOT );
		break;
	}

	cls.scrshot_action = scrshot_inactive;
	cls.shotname[0] = '\0';
}

void SCR_DrawPlaque( void )
{
	shader_t		levelshot;

	if( !re || !cls.drawplaque ) return;

	levelshot = re->RegisterShader( cl_levelshot_name->string, SHADER_NOMIP );
	SCR_DrawPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, levelshot );
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
	case ca_disconnected:
		break;
	case ca_connecting:
	case ca_connected:
		SCR_DrawPlaque();
		CL_DrawHUD( CL_LOADING );
		break;
	case ca_active:
		V_RenderView();
		CL_DrawHUD( CL_ACTIVE );
		CL_DrawDemoRecording();
		break;
	case ca_cinematic:
		SCR_DrawCinematic();
		break;
	default:
		Host_Error( "SCR_UpdateScreen: bad cls.state\n" );
		break;
	}
	V_PostRender();
}

void SCR_RegisterShaders( void )
{
	if( re )
	{
		// register console images
		cls.consoleFont = re->RegisterShader( va( "gfx/fonts/%s", con_font->string ), SHADER_FONT );
		cls.clientFont = re->RegisterShader( va( "gfx/fonts/%s", cl_font->string ), SHADER_FONT );
		cls.netIcon = re->RegisterShader( "#net.png", SHADER_NOMIP ); // internal recource
		cls.fillShader = re->RegisterShader( "*white", SHADER_FONT ); // used for FillRGBA
		cls.particle = re->RegisterShader( "*particle", SHADER_FONT ); // Q1 particlefont

		if( host.developer )
			cls.consoleBack = re->RegisterShader( "gfx/conback", SHADER_NOMIP );
		else cls.consoleBack = re->RegisterShader( "gfx/loading", SHADER_NOMIP );

		// TODO: load a font with a variable charwidths
		Mem_Set( &clgame.ds, 0, sizeof( clgame.ds )); // reset a draw state
		clgame.ds.hHudFont = re->RegisterShader( "gfx/creditsfont", SHADER_NOMIP );
	}

	// vid_state has changed
	if( clgame.hInstance ) clgame.dllFuncs.pfnVidInit();

	g_console_field_width = scr_width->integer / SMALLCHAR_WIDTH - 2;
	g_consoleField.widthInChars = g_console_field_width;
	cls.drawplaque = true;
}

/*
==================
SCR_Init
==================
*/
void SCR_Init( void )
{
	if( scr_init ) return;

	// must be init before startup video subsystem
	scr_width = Cvar_Get( "width", "640", 0, "screen width" );
	scr_height = Cvar_Get( "height", "480", 0, "screen height" );

	scr_centertime = Cvar_Get( "scr_centertime", "2.5", 0, "centerprint hold time" );
	scr_printspeed = Cvar_Get( "scr_printspeed", "8", 0, "centerprint speed of print" );
	cl_levelshot_name = Cvar_Get( "cl_levelshot_name", MAP_DEFAULT_SHADER, 0, "contains path to current levelshot" );
	scr_loading = Cvar_Get("scr_loading", "0", 0, "loading bar progress" );
	scr_download = Cvar_Get("scr_download", "0", 0, "downloading bar progress" );
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0, "test client entities" );
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0, "test dynamic lights" );
	cl_testflashlight = Cvar_Get ("cl_testflashlight", "0", 0, "test generic flashlight" );
	cl_envshot_size = Cvar_Get( "cl_envshot_size", "256", CVAR_ARCHIVE, "envshot size of cube side" );
	cl_font = Cvar_Get( "cl_font", "default", CVAR_ARCHIVE, "in-game messages font" );
	
	// register our commands
	Cmd_AddCommand( "timerefresh", SCR_TimeRefresh_f, "turn quickly and print rendering statistcs" );
	Cmd_AddCommand( "loading", SCR_Loading_f, "prepare client to a loading new map" );
	Cmd_AddCommand( "skyname", CL_SetSky_f, "set new skybox by basename" );
	Cmd_AddCommand( "setfont", CL_SetFont_f, "set console/messsages font" );
	Cmd_AddCommand( "viewpos", SCR_Viewpos_f, "prints current player origin" );

	SCR_RegisterShaders();
	UI_Init();
	UI_SetActiveMenu( UI_MAINMENU );
	SCR_InitCinematic();

	scr_init = true;
}

void SCR_Shutdown( void )
{
	if( !scr_init ) return;

	Cmd_RemoveCommand( "timerefresh" );
	Cmd_RemoveCommand( "loading" );
	Cmd_RemoveCommand( "skyname" );
	Cmd_RemoveCommand( "setfont" );
	Cmd_RemoveCommand( "viewpos" );

	UI_SetActiveMenu( UI_CLOSEMENU );
	UI_Shutdown();
	scr_init = false;
}