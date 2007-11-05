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
// r_misc.c

#include <time.h>
#include "gl_local.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte		data[8][8][4];
	rgbdata_t 	r_tex;

	memset(&r_tex, 0, sizeof(r_tex));

	// particle texture
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	r_tex.width = 8;
	r_tex.height = 8;
	r_tex.type = PF_RGBA_GN; // generated
	r_tex.flags = IMAGE_HAS_ALPHA;
	r_tex.numMips = 1;
	r_tex.palette = NULL;
	r_tex.buffer = (byte *)data;

	r_particletexture = R_LoadImage("***particle***", &r_tex, it_sprite );

	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}
	r_tex.flags &= ~IMAGE_HAS_ALPHA;// notexture don't have alpha
	r_notexture = R_LoadImage("***r_notexture***", &r_tex, it_wall );

	r_radarmap = R_FindImage("common/radarmap", NULL, 0, it_pic);
	r_around = R_FindImage("common/around", NULL, 0, it_pic);

	R_Bloom_InitTextures();
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/

/*
====================
Log_Timestamp
====================
*/
const char* CurTime( void )
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Apr2007-03(23.31.55)");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%b%Y-%d(%H.%M.%S)", crt_tm);
          strcpy( timestamp, timestring );

	return timestamp;
}
 
/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	byte		*buffer;
	char		picname[80]; 
	char		checkname[MAX_OSPATH];
	int		i, c, temp;

	// find a file name to save it to 
	strcpy(picname,"shot00.tga");

	for (i = 0; i <= 99; i++) 
	{ 
		picname[4] = i/10 + '0'; 
		picname[5] = i%10 + '0'; 
		sprintf (checkname, "screenshots/%s", picname);
		if(!FS_FileExists( checkname )) break; // file doesn't exist
	} 
	
	if (i == 100) 
	{
		Msg("SCR_ScreenShot_f: Couldn't create a file\n"); 
		return;
 	}
          
          //UNDONE: make folder with name of current map and save screenshot here
	//sprintf (checkname, "screenshots/%s.tga", CurTime());

	buffer = (byte *)Mem_Alloc(r_temppool, vid.width*vid.height*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	buffer[16] = 24;		// pixel size

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 ); 

	// swap rgb to bgr
	c = 18 + vid.width * vid.height * 3;
	for (i = 18; i < c; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	FS_WriteFile( checkname, buffer, c );

	Mem_Free(buffer);
	Msg("Wrote %s\n", picname);
}