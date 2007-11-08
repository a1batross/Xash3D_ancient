//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       cl_cmds.c - client console commnds
//=======================================================================

#include "client.h"

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
		sprintf( filename, "screenshots/%s/shot9999.tga", cl.configstrings[CS_NAME] );
		return;
	}

	a = lastnum / 1000;
	lastnum -= a * 1000;
	b = lastnum / 100;
	lastnum -= b * 100;
	c = lastnum / 10;
	lastnum -= c * 10;
	d = lastnum;

	sprintf( filename, "screenshots/%s/shot%i%i%i%i.tga", cl.configstrings[CS_NAME], a, b, c, d );
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
	sprintf( checkname, "graphics/background/%s.tga", cl.configstrings[CS_NAME] );
	if(!FS_FileExists( checkname )) re->ScrShot( checkname, true );

}