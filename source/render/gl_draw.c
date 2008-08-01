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

image_t	*draw_chars;
/*
=============
Draw_FindPic
=============
*/
image_t *Draw_FindPic( const char *name )
{
	string		fullname;
	const byte	*buffer = NULL;
	size_t		bufsize = 0;		

	//HACKHACK: use default font
	if(com.stristr(name, "fonts" ))
		buffer = FS_LoadInternal( "default.dds", &bufsize );

	com.snprintf( fullname, MAX_STRING, "gfx/%s", name );
	return R_FindImage( fullname, buffer, bufsize, it_pic );
}


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
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
	pglDrawArrays(GL_QUADS, 0, 4);
	GL_UnlockArrays();
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, const char *pic)
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
void Draw_StretchPic (float x, float y, float w, float h, float s1, float t1, float s2, float t2, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic( pic );
	if(!gl) return;
	GL_Bind( gl->texnum[0] );

	GL_EnableBlend();
	GL_TexEnv( GL_MODULATE );
	if( gl_state.draw_color[3] != 1.0f )
	{
		GL_DisableAlphaTest();
		pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else 
	{
		GL_EnableAlphaTest();
		pglBlendFunc(GL_ONE, GL_ZERO);
	}

	pglColor4fv( gl_state.draw_color );
	VA_SetElem2(tex_array[0], s1, t1);
	VA_SetElem2(vert_array[0], x, y);
	VA_SetElem2(tex_array[1], s2, t1);
	VA_SetElem2(vert_array[1], x+w, y);
	VA_SetElem2(tex_array[2], s2, t2);
	VA_SetElem2(vert_array[2], x+w, y+h);
	VA_SetElem2(tex_array[3], s1, t2);
	VA_SetElem2(vert_array[3], x, y+h);

	GL_LockArrays( 4 );
	pglDrawArrays(GL_QUADS, 0, 4);
	GL_UnlockArrays();

	GL_DisableBlend();
	GL_EnableAlphaTest();
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl) return;

	GL_Bind (gl->texnum[0]);
	pglBegin (GL_QUADS);
	pglTexCoord2f (0, 0);
	pglVertex2f (x, y);
	pglTexCoord2f (1, 0);
	pglVertex2f (x+gl->width, y);
	pglTexCoord2f (1, 1);
	pglVertex2f (x+gl->width, y+gl->height);
	pglTexCoord2f (0, 1);
	pglVertex2f (x, y+gl->height);
	pglEnd ();
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
	pglBegin (GL_QUADS);
	pglTexCoord2f (x/64.0, y/64.0);
	pglVertex2f (x, y);
	pglTexCoord2f ( (x+w)/64.0, y/64.0);
	pglVertex2f (x+w, y);
	pglTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	pglVertex2f (x+w, y+h);
	pglTexCoord2f ( x/64.0, (y+h)/64.0 );
	pglVertex2f (x, y+h);
	pglEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(float x, float y, float w, float h)
{
	pglDisable (GL_TEXTURE_2D);
	pglColor4fv(gl_state.draw_color);
	GL_EnableBlend();
	if(gl_state.draw_color[3] != 1.0f )
		pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	else pglBlendFunc(GL_ONE, GL_ZERO);

	pglBegin (GL_QUADS);
		pglVertex2f(x, y);
		pglVertex2f(x + w, y);
		pglVertex2f(x + w, y + h);
		pglVertex2f(x, y + h);
	pglEnd();

	GL_DisableBlend();
	pglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	pglEnable (GL_BLEND);
	pglDisable (GL_TEXTURE_2D);
	pglColor4f (0, 0, 0, 0.8f);
	pglBegin (GL_QUADS);

	pglVertex2f (0,0);
	pglVertex2f (r_width->integer, 0);
	pglVertex2f (r_width->integer, r_height->integer);
	pglVertex2f (0, r_height->integer);

	pglEnd ();
	pglColor4f (1,1,1,1);
	pglEnable (GL_TEXTURE_2D);
	pglDisable (GL_BLEND);
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

	pglFinish();
	// make sure rows and cols are powers of 2
	for( i = 0;(1<<i) < cols; i++ );
	for( j = 0;(1<<j) < rows; j++ );
	if ((1<<i ) != cols || ( 1<<j ) != rows)
		Host_Error( "Draw_StretchRaw: size not a power of 2: %i by %i\n", cols, rows );
	GL_Bind( 0 );
	if( dirty ) pglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	R_SetGL2D();

	pglColor4fv(gl_state.draw_color);
	pglBegin(GL_QUADS);
	pglTexCoord2f( 0.5f / cols,  0.5f / rows );
	pglVertex2f (x, y);
	pglTexCoord2f((cols - 0.5f) / cols ,  0.5f / rows );
	pglVertex2f (x+w, y);
	pglTexCoord2f((cols - 0.5f) / cols, (rows - 0.5f) / rows );
	pglVertex2f (x+w, y+h);
	pglTexCoord2f(0.5f / cols, ( rows - 0.5f )/rows );
	pglVertex2f (x, y+h);
	pglEnd();
}