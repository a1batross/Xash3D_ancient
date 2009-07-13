//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       cl_cmds.c - client console commnds
//=======================================================================

#include "common.h"
#include "client.h"

#define SCRSHOT_TYPE	"tga"

/*
================
SCR_Loading_f

loading
================
*/
void SCR_Loading_f( void )
{
	S_StopAllSounds();
}


/*
====================
CL_SetFont_f

setfont <fontname>
====================
*/
void CL_SetFont_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: setfont <fontname> <console>\n" );
		return;
	}

	switch(Cmd_Argc( ))
	{
	case 2:
		Cvar_Set( "cl_font", Cmd_Argv( 1 ));
		cls.clientFont = re->RegisterShader( va( "gfx/fonts/%s", cl_font->string ), SHADER_FONT );
		break;
	case 3:
		Cvar_Set( "con_font", Cmd_Argv( 1 ));
		cls.consoleFont = re->RegisterShader( va( "gfx/fonts/%s", con_font->string ), SHADER_FONT );
		break;
	default:
		Msg( "setfont: invalid aruments\n" );
		break;
	}
}

/*
====================
CL_PlayVideo_f

movie <moviename>
====================
*/
void CL_PlayVideo_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "movie <moviename>\n" );
		return;
	}
	if( cls.state == ca_active )
	{
		// FIXME: get rid of this stupid alias
		Cbuf_AddText(va("killserver\n; wait\n; movie %s\n;", Cmd_Argv(1)));
		return;
	}
	SCR_PlayCinematic( Cmd_Argv(1), 0 );
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: download <filename>\n" );
		return;
	}
	CL_CheckOrDownloadFile(Cmd_Argv(1));
}

/* 
================== 
CL_ScreenshotGetName
================== 
*/  
void CL_ScreenshotGetName( int lastnum, char *filename )
{
	int	a, b, c, d;

	if( !filename ) return;
	if( lastnum < 0 || lastnum > 9999 )
	{
		// bound
		com.sprintf( filename, "scrshots/%s/shot9999.%s", cl.configstrings[CS_NAME], SCRSHOT_TYPE );
		return;
	}

	a = lastnum / 1000;
	lastnum -= a * 1000;
	b = lastnum / 100;
	lastnum -= b * 100;
	c = lastnum / 10;
	lastnum -= c * 10;
	d = lastnum;

	com.sprintf( filename, "scrshots/%s/shot%i%i%i%i.%s", cl.configstrings[CS_NAME], a, b, c, d, SCRSHOT_TYPE );
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/
/* 
================== 
CL_ScreenShot_f

normal screenshot
================== 
*/  
void CL_ScreenShot_f( void ) 
{
	int	i;
	string	checkname;

	// scan for a free filename
	for( i = 0; i <= 9999; i++ )
	{
		CL_ScreenshotGetName( i, checkname );
		if( !FS_FileExists( checkname )) break;
	}

	Con_ClearNotify();
	re->ScrShot( checkname, false );
}

void CL_EnvShot_f( void )
{
	string	basename;

	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: envshot <shotname>\n" );
		return;
	}

	Con_ClearNotify();
	com.snprintf( basename, MAX_STRING, "gfx/env/%s", Cmd_Argv( 1 ));
	re->EnvShot( basename, cl_envshot_size->integer, false );
}

void CL_SkyShot_f( void )
{
	string	basename;

	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: envshot <shotname>\n" );
		return;
	}

	Con_ClearNotify();
	com.snprintf( basename, MAX_STRING, "gfx/env/%s", Cmd_Argv( 1 ));
	re->EnvShot( basename, cl_envshot_size->integer, true );
}

/* 
================== 
CL_LevelShot_f

splash logo while map is loading
================== 
*/ 
void CL_LevelShot_f( void )
{
	string	checkname;	

	if( !cl.need_levelshot ) return;

	// check for exist
	com.sprintf( checkname, "media/background/%s.png", cl.configstrings[CS_NAME] );
	if( !FS_FileExists( checkname )) re->ScrShot( checkname, true );
	cl.need_levelshot = false; // done
}

/*
=================
CL_SetSky_f

Set a specific sky and rotation speed
=================
*/
void CL_SetSky_f( void )
{
	if(Cmd_Argc() < 2)
	{
		Msg( "Usage: sky <shadername>\n" );
		return;
	}
	re->RegisterShader( Cmd_Argv(1), SHADER_SKYBOX );
}

/*
================
SCR_TimeRefresh_f

timerefres [noflip]
================
*/
void SCR_TimeRefresh_f( void )
{
	int		i;
	double		start, stop;
	double		time;

	if ( cls.state != ca_active )
		return;

	start = Sys_DoubleTime();

	if( Cmd_Argc() == 2 )
	{	
		// run without page flipping
		re->BeginFrame();
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i / 128.0 * 360.0f;
			re->RenderFrame( &cl.refdef );
		}
		re->EndFrame();
	}
	else
	{
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i / 128.0 * 360.0f;

			re->BeginFrame();
			re->RenderFrame( &cl.refdef );
			re->EndFrame();
		}
	}

	stop = Sys_DoubleTime ();
	time = (stop - start);
	Msg( "%f seconds (%f fps)\n", time, 128 / time );
}

/*
=============
SCR_Viewpos_f

viewpos
=============
*/
void SCR_Viewpos_f( void )
{
	Msg( "( %g %g %g )\n", cl.refdef.vieworg[0], cl.refdef.vieworg[1], cl.refdef.vieworg[2] );
}