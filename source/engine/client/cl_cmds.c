//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       cl_cmds.c - client console commnds
//=======================================================================

#include "common.h"
#include "client.h"

#define SCRSHOT_TYPE	"jpg"

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
	SCR_PlayCinematic( Cmd_Argv( 1 ));
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
	re->ScrShot( checkname, VID_SCREENSHOT );
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
	com.snprintf( basename, MAX_STRING, "env/%s", Cmd_Argv( 1 ));
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
	com.snprintf( basename, MAX_STRING, "env/%s", Cmd_Argv( 1 ));
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
	if( cls.scrshot_request != scrshot_plaque ) return;
	cls.scrshot_request = scrshot_inactive;

	// check for exist
	com.sprintf( cls.shotname, "levelshots/%s.jpg", cl.configstrings[CS_NAME] );
	if( !FS_FileExists( cls.shotname ))
		cls.scrshot_action = scrshot_plaque;	// build new frame for levelshot
	else cls.scrshot_action = scrshot_inactive;	// disable - not needs
}

/* 
================== 
CL_SaveShot_f

mini-pic in loadgame menu
================== 
*/ 
void CL_SaveShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: saveshot <savename>\n" );
		return;
	}

	// check for exist
	com.sprintf( cls.shotname, "save/%s.jpg", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_savegame;		// build new frame for saveshot
}

/* 
================== 
CL_DemoShot_f

mini-pic in playdemo menu
================== 
*/ 
void CL_DemoShot_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: demoshot <savename>\n" );
		return;
	}

	// check for exist
	com.sprintf( cls.shotname, "demos/%s.jpg", Cmd_Argv( 1 ));
	cls.scrshot_action = scrshot_demoshot; // build new frame for saveshot
}

/*
==============
CL_DeleteDemo_f

==============
*/
void CL_DeleteDemo_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: deldemo <name>\n" );
		return;
	}

	if( cls.demorecording && !com.stricmp( cls.demoname, Cmd_Argv( 1 )))
	{
		Msg( "Can't delete %s - recording\n", Cmd_Argv( 1 ));
		return;
	}

	// delete save and saveshot
	FS_Delete( va( "%s/demos/%s.dem", GI->gamedir, Cmd_Argv( 1 )));
	FS_Delete( va( "%s/demos/%s.jpg", GI->gamedir, Cmd_Argv( 1 )));
}

/*
=================
CL_SetSky_f

Set a specific sky and rotation speed
=================
*/
void CL_SetSky_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: sky <shadername>\n" );
		return;
	}
	re->RegisterShader( Cmd_Argv(1), SHADER_SKY );
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

	if( cls.state != ca_active )
		return;

	start = Sys_DoubleTime();

	if( Cmd_Argc() == 2 )
	{	
		// run without page flipping
		re->BeginFrame( false );
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

			re->BeginFrame( true );
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
	Msg( "org ( %g %g %g )\n", cl.refdef.vieworg[0], cl.refdef.vieworg[1], cl.refdef.vieworg[2] );
	Msg( "ang ( %g %g %g )\n", cl.refdef.viewangles[0], cl.refdef.viewangles[1], cl.refdef.viewangles[2] );
}