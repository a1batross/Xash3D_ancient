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
// screen.h
#ifndef SCREEN_H
#define SCREEN_H

typedef struct vrect_s
{
	int	x;
	int	y;
	int	width;
	int	height;
} vrect_t;

extern vrect_t scr_vrect;	// position of render window

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

#define TINYCHAR_WIDTH	(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT	(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH	8
#define SMALLCHAR_HEIGHT	16
#define BIGCHAR_WIDTH	16
#define BIGCHAR_HEIGHT	24
#define GIANTCHAR_WIDTH	32
#define GIANTCHAR_HEIGHT	48

void SCR_Init (void);
void SCR_UpdateScreen (void);
void VID_Init( void );
void SCR_AdjustSize( float *x, float *y, float *w, float *h );
void SCR_DrawPic( float x, float y, float width, float height, const char *picname );
void SCR_FillRect( float x, float y, float width, float height, const float *color );
void SCR_DrawSmallChar( int x, int y, int ch );
void SCR_DrawChar( int x, int y, float w, float h, int ch );
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, bool forceColor );
void SCR_DrawStringExt( int x, int y, float w, float h, const char *string, float *setColor, bool forceColor );
void SCR_DrawBigString( int x, int y, const char *s, float alpha );
void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color );
void SCR_DrawFPS( void );
void SCR_DrawNet( void );

#endif//SCREEN_H