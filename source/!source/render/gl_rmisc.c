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
R_CreateNoTexture

Quake3 style notexture
==================
*/
static void R_CreateNoTexture( void )
{
	int		x;
	byte		data[16][16][4];
	rgbdata_t 	r_tex;

	// the default image will be a box, to allow you to see the mapping coordinates
	memset( data, 32, sizeof(data));
	memset(&r_tex, 0, sizeof(r_tex));
	
	for ( x = 0; x < 16; x++ )
	{
		data[0][x][0] = data[0][x][1] = data[0][x][2] = data[0][x][3] = 255;
		data[x][0][0] = data[x][0][1] = data[x][0][2] = data[x][0][3] = 255;
		data[15][x][0] = data[15][x][1] = data[15][x][2] = data[15][x][3] = 255;
		data[x][15][0] = data[x][15][1] = data[x][15][2] = data[x][15][3] = 255;
	}

	r_tex.width = 16;
	r_tex.height = 16;
	r_tex.type = PF_RGBA_GN; // generated
	r_tex.size = r_tex.width * r_tex.height * 4;
	r_tex.numMips = 1;
	r_tex.buffer = (byte *)data;
	r_notexture = R_LoadImage("*r_notexture", &r_tex, it_wall );
}

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
	r_tex.size = r_tex.width * r_tex.height * 4;
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