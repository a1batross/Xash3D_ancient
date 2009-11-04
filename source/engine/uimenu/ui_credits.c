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

#include "common.h"
#include "ui_local.h"
#include "client.h"

#define ART_BACKGROUND		"gfx/shell/credits"
#define UI_CREDITS_PATH		"scripts/credits.txt"
#define UI_CREDITS_MAXLINES		2048

static const char *uiCreditsDefault[] = 
{
	"^3Xash3D",
	"",
	"^3PROGRAMMING",
	"Uncle Mike",
	"",
	"^3ART",
	"Chorus",
	"Small Link",
	"",
	"^3LEVEL DESIGN",
	"Scrama",
	"Mitoh",
	"",
	"",
	"^3SPECIAL THANKS",
	"Chain Studios at all",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3MUSIC",
	"Dj Existence",
	"",
	"",
	"^3THANKS TO",
	"ID Software at all",
	"Georg Destroy for icon graphics",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3Xash3D using some parts of:",
	"Doom 1 (Id Software)",
	"Quake 1 (Id Software)",
	"Quake 2 (Id Software)",
	"Quake 3 (Id Software)",
	"Half-Life (Valve Software)",
	"Darkplaces (Darkplaces Team)",
	"Quake 2 Evolved (Team Blur)",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Copyright XashXT Group 2009 (C)",
	0
};

typedef struct
{
	const char	**credits;
	int		startTime;
	float		showTime;
	float		fadeTime;
	int		numLines;
	bool		active;
	char		*index[UI_CREDITS_MAXLINES];
	char		*buffer;

	menuFramework_s	menu;
} uiCredits_t;

static uiCredits_t		uiCredits;


/*
=================
UI_Credits_DrawFunc
=================
*/
static void UI_Credits_DrawFunc( void )
{
	int	i, y;
	int	w = BIGCHAR_WIDTH;
	int	h = BIGCHAR_HEIGHT;
	rgba_t	color = { 0, 0, 0, 0 };

	// draw the background first
	UI_DrawPic( 0, 0, 1024 * uiStatic.scaleX, 768 * uiStatic.scaleY, uiColorWhite, ART_BACKGROUND );

	// now draw the credits
	UI_ScaleCoords( NULL, NULL, &w, &h );

	y = scr_height->integer - ((uiStatic.realTime - uiCredits.startTime) / 40.0f);

	// draw the credits
	for ( i = 0; i < uiCredits.numLines && uiCredits.credits[i]; i++, y += 20 )
	{
		// skip not visible lines, but always draw end line
		if( y <= -16 && i != uiCredits.numLines - 1 ) continue;

		if(( y < (scr_height->integer - h) / 2 ) && i == uiCredits.numLines - 1 )
		{
			if( !uiCredits.fadeTime ) uiCredits.fadeTime = (uiStatic.realTime * 0.001f);
			CL_FadeAlpha( uiCredits.fadeTime, uiCredits.showTime, color );
			if( color[3] ) UI_DrawString( 0, (scr_height->integer - h) / 2, 1024 * uiStatic.scaleX, h, uiCredits.credits[i], color, true, w, h, 1, true );
		}
		else UI_DrawString( 0, y, 1024 * uiStatic.scaleX, h, uiCredits.credits[i], uiColorWhite, false, w, h, 1, true );
	}

	if( y < 0 && !color[3] )
	{
		uiCredits.active = false; // end of credits
		UI_PopMenu();
	}
}

/*
=================
UI_Credits_KeyFunc
=================
*/
static const char *UI_Credits_KeyFunc( int key )
{
	uiCredits.active = false;
	UI_PopMenu();
	return uiSoundNull;
}

/*
=================
UI_Credits_Init
=================
*/
static void UI_Credits_Init( void )
{
	uiCredits.menu.drawFunc = UI_Credits_DrawFunc;
	uiCredits.menu.keyFunc = UI_Credits_KeyFunc;

	if( !uiCredits.buffer )
	{
		int	count;
		char	*p;

		// load credits if needed
		uiCredits.buffer = FS_LoadFile( UI_CREDITS_PATH, &count );
		if( count )
		{
			if( uiCredits.buffer[count - 1] != '\n' && uiCredits.buffer[count - 1] != '\r' )
			{
				uiCredits.buffer = Mem_Realloc( cls.mempool, uiCredits.buffer, count + 2 );
				com.strncpy( uiCredits.buffer + count, "\r", 1 ); // add terminator
				count += 2; // added "\r\0"
                    	}

			p = uiCredits.buffer;

			// convert customs credits to 'ideal' strings array
			for( uiCredits.numLines = 0; uiCredits.numLines < UI_CREDITS_MAXLINES; uiCredits.numLines++ )
			{
				uiCredits.index[uiCredits.numLines] = p;
				while( *p != '\r' && *p != '\n' )
				{
					p++;
					if( --count == 0 ) break;
				}
				if( *p == '\r' )
				{
					*p++ = 0;
					if( --count == 0 ) break;
				}
				*p++ = 0;
				if( --count == 0 ) break;
			}
			uiCredits.index[++uiCredits.numLines] = 0;
			uiCredits.credits = uiCredits.index;
		}
		else
		{
			// use built-in credits
			uiCredits.credits =  uiCreditsDefault;
			uiCredits.numLines = ( sizeof( uiCreditsDefault ) / sizeof( uiCreditsDefault[0] )) - 1; // skip term
		}
	}

	// run credits
	uiCredits.startTime = uiStatic.realTime + 500; // make half-seconds delay
	uiCredits.showTime = bound( 0.1f, com.strlen( uiCredits.credits[uiCredits.numLines - 1]), 12.0f );
	uiCredits.fadeTime = 0.0f; // will be determined later
	uiCredits.active = true;
}

bool UI_CreditsActive( void )
{
	return uiCredits.active;
}

/*
=================
UI_Credits_Precache
=================
*/
void UI_Credits_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
}

/*
=================
UI_Credits_Menu
=================
*/
void UI_Credits_Menu( void )
{
	UI_Credits_Precache();
	UI_Credits_Init();

	UI_PushMenu( &uiCredits.menu );
}