//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       cl_cmds.c - client console commnds
//=======================================================================

#include "common.h"
#include "client.h"

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	S_StopAllSounds();
}

void CL_SetFont_f( void )
{
	if(Cmd_Argc() < 2)
	{
		Msg("Usage: setfont <fontname>\n");
		return;
	}
	Cvar_Set("cl_font", Cmd_Argv(1));
}

/* 
================== 
CL_ScreenshotGetName
================== 
*/  
void CL_ScreenshotGetName( int lastnum, char *filename )
{
	int	a, b, c, d;

	if(!filename) return;
	if(lastnum < 0 || lastnum > 9999)
	{
		// bound
		com.sprintf( filename, "scrshots/%s/shot9999.dds", cl.configstrings[CS_NAME] );
		return;
	}

	a = lastnum / 1000;
	lastnum -= a * 1000;
	b = lastnum / 100;
	lastnum -= b * 100;
	c = lastnum / 10;
	lastnum -= c * 10;
	d = lastnum;

	com.sprintf( filename, "scrshots/%s/shot%i%i%i%i.dds", cl.configstrings[CS_NAME], a, b, c, d );
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
	int		i;
	char		checkname[MAX_OSPATH];

	// scan for a free filename
	for (i = 0; i <= 9999; i++ )
	{
		CL_ScreenshotGetName( i, checkname );
		if(!FS_FileExists( checkname )) break;
	}

	Con_ClearNotify();
	re->ScrShot( checkname, false );
}

/* 
================== 
CL_LevelShot_f

splash logo while map is loading
================== 
*/ 
void CL_LevelShot_f( void )
{
	char		checkname[MAX_OSPATH];	

	// check for exist
	com.sprintf( checkname, "gfx/background/%s.dds", cl.configstrings[CS_NAME] );
	if(!FS_FileExists( checkname )) re->ScrShot( checkname, true );
	else Msg("levelshot for this map already created\nFirst remove old image if you wants do it again\n" );
}

/*
=================
CL_SetSky_f

Set a specific sky and rotation speed
=================
*/
void CL_SetSky_f( void )
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
================
SCR_TimeRefresh_f
================
*/
void SCR_TimeRefresh_f( void )
{
	int		i;
	int		start, stop;
	float		time;

	if ( cls.state != ca_active )
		return;

	start = Sys_Milliseconds();

	if( Cmd_Argc() == 2 )
	{	
		// run without page flipping
		re->BeginFrame();
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i/128.0 * 360.0f;
			re->RenderFrame (&cl.refdef);
		}
		re->EndFrame();
	}
	else
	{
		for( i = 0; i < 128; i++ )
		{
			cl.refdef.viewangles[1] = i/128.0 * 360.0f;

			re->BeginFrame();
			re->RenderFrame(&cl.refdef);
			re->EndFrame();
		}
	}

	stop = Sys_Milliseconds();
	time = (stop - start) / 1000.0f;
	Msg( "%f seconds (%f fps)\n", time, 128 / time );
}