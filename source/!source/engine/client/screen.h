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

#include "vid.h"

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

#define TINYCHAR_WIDTH	(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT	(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH	8
#define SMALLCHAR_HEIGHT	16
#define BIGCHAR_WIDTH	16
#define BIGCHAR_HEIGHT	16
#define GIANTCHAR_WIDTH	32
#define GIANTCHAR_HEIGHT	48

// cinematic states
typedef enum
{
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

// cinematic flags
#define CIN_loop		1
#define CIN_hold		2
#define CIN_silent		4

#define COLOR_0		NULL
#define COLOR_4		GetRGBA(1.0f, 0.5f, 0.0f, 1.0f)
#define COLOR_8		GetRGBA(1.0f, 0.5f, 1.0f, 1.0f)
#define COLOR_16		GetRGBA(0.3f, 0.8f, 1.0f, 1.0f)
#define COLOR_64		GetRGBA(0.7f, 0.5f, 0.0f, 1.0f)
#define COLOR_208		GetRGBA(0.0f, 0.4f, 0.0f, 1.0f)
#define COLOR_223		GetRGBA(0.5f, 0.2f, 1.0f, 1.0f)

void	SCR_Init (void);

void	SCR_UpdateScreen (void);

void	SCR_SizeUp (void);
void	SCR_SizeDown (void);
void	SCR_CenterPrint (char *str);
void	SCR_BeginLoadingPlaque (void);
void	SCR_EndLoadingPlaque (void);

void	SCR_TouchPics (void);

void	SCR_RunConsole (void);

extern	int			sb_lines;

extern	cvar_t		*crosshair;

extern	vrect_t		scr_vrect;		// position of render window

extern	char		crosshair_pic[MAX_QPATH];
extern	int			crosshair_width, crosshair_height;

void SCR_AddDirtyPoint (int x, int y);
void SCR_DirtyScreen (void);
void SCR_AdjustSize( float *x, float *y, float *w, float *h );
void SCR_DrawPic( float x, float y, float width, float height, char *picname );
void SCR_FillRect( float x, float y, float width, float height, const float *color );
void SCR_DrawSmallChar( int x, int y, int ch );
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, bool forceColor );
void SCR_DrawStringExt( int x, int y, float size, const char *string, float *setColor, bool forceColor );
void SCR_DrawBigString( int x, int y, const char *s, float alpha );
void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color );

//
// cl_user.c
//
void CG_SetSky_f( void );
void CG_DrawCenterString( void );
void CG_CenterPrint( const char *str, int y, int charWidth );
void CG_DrawCenterPic( int w, int h, char *picname );
void CG_ExecuteProgram( char *section );
void CG_MakeLevelShot( void );
void CG_DrawLoading( void );
void CG_DrawNet( void );
void CG_DrawPause( void );
void CG_Init( void );

#endif//SCREEN_H