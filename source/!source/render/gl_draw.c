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

// draw.c

#include "gl_local.h"

image_t		*draw_chars;

byte def_font[] =
{
#include "lhfont.h"
};

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	char	fullname[MAX_QPATH];

	sprintf (fullname, "graphics/%s", name);
	return R_FindImage (fullname, NULL, 0, it_pic);
}


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	draw_chars = R_FindImage("graphics/fonts/conchars", def_font, sizeof(def_font), it_pic);

	GL_Bind( draw_chars->texnum[0] );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Char(x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_Char (float x, float y, int num)
{
	int		row, col;
	float		frow, fcol, size;

	num &= 255;
	
	if ( (num & 127) == 32 ) return;// space

	if (y <= -8) return; // totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind (draw_chars->texnum[0]);

	VA_SetElem2(tex_array[0],fcol, frow);
	VA_SetElem2(vert_array[0],x, y);
	VA_SetElem2(tex_array[1],fcol + size, frow);
	VA_SetElem2(vert_array[1],x+8, y);
	VA_SetElem2(tex_array[2],fcol + size, frow + size);
	VA_SetElem2(vert_array[2],x+8, y+8);
	VA_SetElem2(tex_array[3],fcol, frow + size);
	VA_SetElem2(vert_array[3],x, y+8);
	
	GL_LockArrays( 4 );
	qglDrawArrays(GL_QUADS, 0, 4);
	GL_UnlockArrays();
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic( pic );
	if(!gl) return;

	GL_Bind (gl->texnum[0]);

	GL_EnableBlend();
	GL_TexEnv( GL_MODULATE );

	if(gl_state.draw_color[3] != 1.0f )
	{
		GL_DisableAlphaTest();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else 
	{
		qglBlendFunc(GL_ONE, GL_ZERO);
	
	}

	qglColor4fv( gl_state.draw_color );
	qglBegin (GL_QUADS);
		qglTexCoord2f (s1, t1);
		qglVertex2f (x, y);
		qglTexCoord2f (s2, t1);
		qglVertex2f (x+w, y);
		qglTexCoord2f (s2, t2);
		qglVertex2f (x+w, y+h);
		qglTexCoord2f (s1, t2);
		qglVertex2f (x, y+h);
	qglEnd ();

	GL_DisableBlend();
	GL_EnableAlphaTest();
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl) return;

	GL_Bind (gl->texnum[0]);
	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+gl->width, y);
	qglTexCoord2f (1, 1);
	qglVertex2f (x+gl->width, y+gl->height);
	qglTexCoord2f (0, 1);
	qglVertex2f (x, y+gl->height);
	qglEnd ();
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image) return;

	GL_Bind (image->texnum[0]);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x/64.0, y/64.0);
	qglVertex2f (x, y);
	qglTexCoord2f ( (x+w)/64.0, y/64.0);
	qglVertex2f (x+w, y);
	qglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( x/64.0, (y+h)/64.0 );
	qglVertex2f (x, y+h);
	qglEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(float x, float y, float w, float h)
{
	qglDisable (GL_TEXTURE_2D);
	qglColor4fv(gl_state.draw_color);
	GL_EnableBlend();

	if(gl_state.draw_color[3] != 1.0f )
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	else qglBlendFunc(GL_ONE, GL_ZERO);

	qglBegin (GL_QUADS);
		qglVertex2f(x, y);
		qglVertex2f(x + w, y);
		qglVertex2f(x + w, y + h);
		qglVertex2f(x, y + h);
	qglEnd();
	GL_DisableBlend();
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.8);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (r_width->integer, 0);
	qglVertex2f (r_width->integer, r_height->integer);
	qglVertex2f (0, r_height->integer);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
}


//====================================================================
/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data, bool dirty)
{
	int	i, j;

	qglFinish();
	// make sure rows and cols are powers of 2
	for( i = 0;(1<<i) < cols; i++ );
	for( j = 0;(1<<j) < rows; j++ );
	if ((1<<i ) != cols || ( 1<<j ) != rows)
		Sys_Error ("Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	GL_Bind(0);
	if( dirty ) qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	GL_TexFilter( false );
	R_SetGL2D();

	qglColor4fv(gl_state.draw_color);
	qglBegin(GL_QUADS);
	qglTexCoord2f( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f((cols - 0.5f) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f((cols - 0.5f) / cols, (rows - 0.5f) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f(0.5f / cols, ( rows - 0.5f )/rows );
	qglVertex2f (x, y+h);
	qglEnd();
}