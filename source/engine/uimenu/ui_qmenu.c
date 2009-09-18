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


// ui_qmenu.c -- Quake menu framework

#include "common.h"
#include "ui_local.h"
#include "input.h"

/*
=================
UI_ScrollList_Init
=================
*/
void UI_ScrollList_Init( menuScrollList_s *sl )
{
	if( !sl->generic.name ) sl->generic.name = "";

	if( sl->generic.flags & QMF_BIGFONT )
	{
		sl->generic.charWidth = UI_BIG_CHAR_WIDTH;
		sl->generic.charHeight = UI_BIG_CHAR_HEIGHT;
	}
	else if( sl->generic.flags & QMF_SMALLFONT )
	{
		sl->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		sl->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}
	else
	{
		if( sl->generic.charWidth < 1 ) sl->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		if( sl->generic.charHeight < 1 ) sl->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}

	UI_ScaleCoords( NULL, NULL, &sl->generic.charWidth, &sl->generic.charHeight );

	if(!(sl->generic.flags & (QMF_LEFT_JUSTIFY|QMF_CENTER_JUSTIFY|QMF_RIGHT_JUSTIFY)))
		sl->generic.flags |= QMF_LEFT_JUSTIFY;

	if( !sl->generic.color ) sl->generic.color = uiColorWhite;
	if( !sl->generic.focusColor ) sl->generic.focusColor = uiColorLtGrey;
	if( !sl->background ) sl->background = UI_BACKGROUNDLISTBOX;
	if( !sl->upArrow ) sl->upArrow = UI_UPARROW;
	if( !sl->upArrowFocus ) sl->upArrowFocus = UI_UPARROWFOCUS;
	if( !sl->downArrow ) sl->downArrow = UI_DOWNARROW;
	if( !sl->downArrowFocus ) sl->downArrowFocus = UI_DOWNARROWFOCUS;

	sl->curItem = 0;
	sl->topItem = 0;
	sl->numItems = 0;

	// count number of items
	while( sl->itemNames[sl->numItems] )
		sl->numItems++;

	// scale the center box
	sl->generic.x2 = sl->generic.x;
	sl->generic.y2 = sl->generic.y;
	sl->generic.width2 = sl->generic.width;
	sl->generic.height2 = sl->generic.height;
	UI_ScaleCoords( &sl->generic.x2, &sl->generic.y2, &sl->generic.width2, &sl->generic.height2 );

	// calculate number of visible rows
	sl->numRows = (sl->generic.height2 / sl->generic.charHeight) - 2;
	if( sl->numRows > sl->numItems ) sl->numRows = sl->numItems;

	// extend the height so it has room for the arrows
	sl->generic.height += (sl->generic.width / 4);

	// calculate new Y for the control
	sl->generic.y -= (sl->generic.width / 8);

	UI_ScaleCoords( &sl->generic.x, &sl->generic.y, &sl->generic.width, &sl->generic.height );
}

/*
=================
UI_ScrollList_Key
=================
*/
const char *UI_ScrollList_Key( menuScrollList_s *sl, int key )
{
	const char	*sound = 0;
	int		arrowWidth, arrowHeight, upX, upY, downX, downY;
	int		i, y;

	switch( key )
	{
	case K_MOUSE1:
		if(!( sl->generic.flags & QMF_HASMOUSEFOCUS ))
			break;

		// calculate size and position for the arrows
		arrowWidth = sl->generic.width / 4;
		arrowHeight = sl->generic.width / 8;

		upX = sl->generic.x + (arrowWidth * 1.5);
		upY = sl->generic.y;
		downX = sl->generic.x + (arrowWidth * 1.5);
		downY = sl->generic.y + (sl->generic.height - arrowHeight);

		// Now see if either up or down has focus
		if( UI_CursorInRect( upX, upY, arrowWidth, arrowHeight ))
		{
			if( sl->curItem != 0 )
			{
				sl->curItem--;
				sound = uiSoundMove;
			}
			else sound = uiSoundBuzz;
			break;
		}
		else if( UI_CursorInRect( downX, downY, arrowWidth, arrowHeight ))
		{
			if( sl->curItem != sl->numItems - 1 )
			{
				sl->curItem++;
				sound = uiSoundMove;
			}
			else sound = uiSoundBuzz;
			break;
		}

		// see if an item has been selected
		y = sl->generic.y2 + sl->generic.charHeight;
		for( i = sl->topItem; i < sl->topItem + sl->numRows; i++, y += sl->generic.charHeight )
		{
			if( !sl->itemNames[i] )
				break; // done

			if( UI_CursorInRect( sl->generic.x, y, sl->generic.width, sl->generic.charHeight ))
			{
				sl->curItem = i;
				sound = uiSoundNull;
				break;
			}
		}
		break;
	case K_HOME:
	case K_KP_HOME:
		if( sl->curItem != 0 )
		{
			sl->curItem = 0;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_END:
	case K_KP_END:
		if( sl->curItem != sl->numItems - 1 )
		{
			sl->curItem = sl->numItems - 1;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_PGUP:
	case K_KP_PGUP:
		if( sl->curItem != 0 )
		{
			sl->curItem -= 2;
			if( sl->curItem < 0 )
				sl->curItem = 0;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_PGDN:
	case K_KP_PGDN:
		if( sl->curItem != sl->numItems - 1 )
		{
			sl->curItem += 2;
			if( sl->curItem > sl->numItems - 1 )
				sl->curItem = sl->numItems - 1;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_UPARROW:
	case K_KP_UPARROW:
		if( sl->curItem != 0 )
		{
			sl->curItem--;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_DOWNARROW:
		if( sl->curItem != sl->numItems - 1 )
		{
			sl->curItem++;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	}

	sl->topItem = sl->curItem - sl->numRows + 1;
	if( sl->topItem < 0 ) sl->topItem = 0;
	if( sl->topItem > sl->numItems - sl->numRows )
		sl->topItem = sl->numItems - sl->numRows;

	if( sound && ( sl->generic.flags & QMF_SILENT ))
		sound = uiSoundNull;

	if( sound && sl->generic.callback )
	{
		if( sound != uiSoundBuzz )
			sl->generic.callback( sl, QM_CHANGED );
	}
	return sound;
}

/*
=================
UI_ScrollList_Draw
=================
*/
void UI_ScrollList_Draw( menuScrollList_s *sl )
{
	int	justify;
	bool	shadow;
	int	i, x, y, w, h;
	int	arrowWidth, arrowHeight, upX, upY, downX, downY;
	bool	upFocus, downFocus;

	if( sl->generic.flags & QMF_LEFT_JUSTIFY )
		justify = 0;
	else if( sl->generic.flags & QMF_CENTER_JUSTIFY )
		justify = 1;
	else if( sl->generic.flags & QMF_RIGHT_JUSTIFY )
		justify = 2;

	shadow = (sl->generic.flags & QMF_DROPSHADOW);

	// calculate size and position for the arrows
	arrowWidth = sl->generic.width / 4;
	arrowHeight = sl->generic.width / 8;

	upX = sl->generic.x + (arrowWidth * 1.5);
	upY = sl->generic.y;
	downX = sl->generic.x + (arrowWidth * 1.5);
	downY = sl->generic.y + (sl->generic.height - arrowHeight);

	// draw the arrows
	if( sl->generic.flags & QMF_GRAYED )
	{
		UI_DrawPic( upX, upY, arrowWidth, arrowHeight, uiColorDkGrey, sl->upArrow );
		UI_DrawPic( downX, downY, arrowWidth, arrowHeight, uiColorDkGrey, sl->downArrow );
	}
	else
	{
		if((menuCommon_s *)sl != (menuCommon_s *)UI_ItemAtCursor(sl->generic.parent))
		{
			UI_DrawPic( upX, upY, arrowWidth, arrowHeight, sl->generic.color, sl->upArrow );
			UI_DrawPic( downX, downY, arrowWidth, arrowHeight, sl->generic.color, sl->downArrow );
		}
		else
		{
			// see which arrow has the mouse focus
			upFocus = UI_CursorInRect( upX, upY, arrowWidth, arrowHeight );
			downFocus = UI_CursorInRect( downX, downY, arrowWidth, arrowHeight );

			if(!( sl->generic.flags & QMF_FOCUSBEHIND ))
			{
				UI_DrawPic( upX, upY, arrowWidth, arrowHeight, sl->generic.color, sl->upArrow );
				UI_DrawPic( downX, downY, arrowWidth, arrowHeight, sl->generic.color, sl->downArrow );
			}

			if( sl->generic.flags & QMF_HIGHLIGHTIFFOCUS )
			{
				UI_DrawPic( upX, upY, arrowWidth, arrowHeight, (upFocus) ? sl->generic.focusColor : sl->generic.color, (upFocus) ? sl->upArrowFocus : sl->upArrow );
				UI_DrawPic( downX, downY, arrowWidth, arrowHeight, (downFocus) ? sl->generic.focusColor : sl->generic.color, (downFocus) ? sl->downArrowFocus : sl->downArrow );
			}
			else if( sl->generic.flags & QMF_PULSEIFFOCUS )
			{
				rgba_t	color;

				*(uint *)color = *(uint *)sl->generic.color;
				color[3] = 255 * (0.5 + 0.5 * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

				UI_DrawPic( upX, upY, arrowWidth, arrowHeight, (upFocus) ? color : sl->generic.color, (upFocus) ? sl->upArrowFocus : sl->upArrow );
				UI_DrawPic( downX, downY, arrowWidth, arrowHeight, (downFocus) ? color : sl->generic.color, (downFocus) ? sl->downArrowFocus : sl->downArrow );
			}
			else if( sl->generic.flags & QMF_BLINKIFFOCUS )
			{
				if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
				{
					UI_DrawPic( upX, upY, arrowWidth, arrowHeight, (upFocus) ? sl->generic.focusColor : sl->generic.color, (upFocus) ? sl->upArrowFocus : sl->upArrow );
					UI_DrawPic( downX, downY, arrowWidth, arrowHeight, (downFocus) ? sl->generic.focusColor : sl->generic.color, (downFocus) ? sl->downArrowFocus : sl->downArrow );
				}
			}

			if( sl->generic.flags & QMF_FOCUSBEHIND )
			{
				UI_DrawPic( upX, upY, arrowWidth, arrowHeight, sl->generic.color, sl->upArrow );
				UI_DrawPic( downX, downY, arrowWidth, arrowHeight, sl->generic.color, sl->downArrow );
			}
		}
	}

	// hightlight the selected item
	if( !( sl->generic.flags & QMF_GRAYED ))
	{
		y = sl->generic.y2 + sl->generic.charHeight;
		for( i = sl->topItem; i < sl->topItem + sl->numRows; i++, y += sl->generic.charHeight )
		{
			if( !sl->itemNames[i] )
				break;		// Done

			if( i == sl->curItem )
			{
				UI_DrawPic( sl->generic.x, y, sl->generic.width, sl->generic.charHeight, uiColorWhite, UI_SELECTIONBOX );
				break;
			}
		}
	}

	// get size and position for the center box
	w = sl->generic.width2;
	h = sl->generic.height2;
	x = sl->generic.x2;
	y = sl->generic.y2;

	UI_DrawPic( x, y, w, h, uiColorWhite, sl->background );

	// Draw the list
	h = sl->generic.charHeight;
	y = sl->generic.y2 + sl->generic.charHeight;
	for( i = sl->topItem; i < sl->topItem + sl->numRows; i++, y += sl->generic.charHeight )
	{
		if( !sl->itemNames[i] )
			break;	// done

		if( sl->generic.flags & QMF_GRAYED )
		{
			UI_DrawString( x, y, w, h, sl->itemNames[i], uiColorDkGrey, true, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
			continue;	// grayed
		}

		if( i != sl->curItem )
		{
			UI_DrawString( x, y, w, h, sl->itemNames[i], sl->generic.color, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
			continue;	// no focus
		}

		if(!( sl->generic.flags & QMF_FOCUSBEHIND ))
			UI_DrawString( x, y, w, h, sl->itemNames[i], sl->generic.color, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );

		if( sl->generic.flags & QMF_HIGHLIGHTIFFOCUS )
			UI_DrawString( x, y, w, h, sl->itemNames[i], sl->generic.focusColor, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
		else if( sl->generic.flags & QMF_PULSEIFFOCUS )
		{
			rgba_t	color;

			*(uint *)color = *(uint *)sl->generic.color;
			color[3] = 255 * (0.5 + 0.5 * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

			UI_DrawString( x, y, w, h, sl->itemNames[i], color, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
		}
		else if( sl->generic.flags & QMF_BLINKIFFOCUS )
		{
			if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
				UI_DrawString( x, y, w, h, sl->itemNames[i], sl->generic.focusColor, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
		}

		if( sl->generic.flags & QMF_FOCUSBEHIND )
			UI_DrawString( x, y, w, h, sl->itemNames[i], sl->generic.color, false, sl->generic.charWidth, sl->generic.charHeight, justify, shadow );
	}
}

/*
=================
UI_SpinControl_Init
=================
*/
void UI_SpinControl_Init( menuSpinControl_s *sc )
{
	if( !sc->generic.name ) sc->generic.name = "";	// this is also the text displayed

	if( sc->generic.flags & QMF_BIGFONT )
	{
		sc->generic.charWidth = UI_BIG_CHAR_WIDTH;
		sc->generic.charHeight = UI_BIG_CHAR_HEIGHT;
	}
	else if( sc->generic.flags & QMF_SMALLFONT )
	{
		sc->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		sc->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}
	else
	{
		if( sc->generic.charWidth < 1 ) sc->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		if( sc->generic.charHeight < 1 ) sc->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}

	UI_ScaleCoords( NULL, NULL, &sc->generic.charWidth, &sc->generic.charHeight );

	if(!( sc->generic.flags & (QMF_LEFT_JUSTIFY|QMF_CENTER_JUSTIFY|QMF_RIGHT_JUSTIFY)))
		sc->generic.flags |= QMF_LEFT_JUSTIFY;

	if( !sc->generic.color ) sc->generic.color = uiColorWhite;
	if( !sc->generic.focusColor ) sc->generic.focusColor = uiColorLtGrey;

	if( !sc->background ) sc->background = UI_BACKGROUNDBOX;

	if( !sc->leftArrow ) sc->leftArrow = UI_LEFTARROW;
	if( !sc->leftArrowFocus ) sc->leftArrowFocus = UI_LEFTARROWFOCUS;
	if( !sc->rightArrow ) sc->rightArrow = UI_RIGHTARROW;
	if( !sc->rightArrowFocus ) sc->rightArrowFocus = UI_RIGHTARROWFOCUS;

	// scale the center box
	sc->generic.x2 = sc->generic.x;
	sc->generic.y2 = sc->generic.y;
	sc->generic.width2 = sc->generic.width;
	sc->generic.height2 = sc->generic.height;
	UI_ScaleCoords( &sc->generic.x2, &sc->generic.y2, &sc->generic.width2, &sc->generic.height2 );

	// extend the width so it has room for the arrows
	sc->generic.width += (sc->generic.height * 3);

	// calculate new X for the control
	sc->generic.x -= (sc->generic.height + (sc->generic.height/2));

	UI_ScaleCoords( &sc->generic.x, &sc->generic.y, &sc->generic.width, &sc->generic.height );
}

/*
=================
UI_SpinControl_Key
=================
*/
const char *UI_SpinControl_Key( menuSpinControl_s *sc, int key )
{
	const char	*sound = 0;
	int		arrowWidth, arrowHeight, leftX, leftY, rightX, rightY;

	switch( key )
	{
	case K_MOUSE1:
	case K_MOUSE2:
		if( !( sc->generic.flags & QMF_HASMOUSEFOCUS ))
			break;

		// calculate size and position for the arrows
		arrowWidth = sc->generic.height;
		arrowHeight = sc->generic.height;

		leftX = sc->generic.x;
		leftY = sc->generic.y;
		rightX = sc->generic.x + (sc->generic.width - arrowWidth);
		rightY = sc->generic.y;

		// now see if either left or right arrow has focus
		if( UI_CursorInRect( leftX, leftY, arrowWidth, arrowHeight ))
		{
			if( sc->curValue > sc->minValue )
			{
				sc->curValue -= sc->range;
				if( sc->curValue < sc->minValue )
					sc->curValue = sc->minValue;
				sound = uiSoundMove;
			}
			else sound = uiSoundBuzz;
		}
		else if( UI_CursorInRect( rightX, rightY, arrowWidth, arrowHeight ))
		{
			if( sc->curValue < sc->maxValue )
			{
				sc->curValue += sc->range;
				if( sc->curValue > sc->maxValue )
					sc->curValue = sc->maxValue;
				sound = uiSoundMove;
			}
			else sound = uiSoundBuzz;
		}
		break;
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		if( sc->generic.flags & QMF_MOUSEONLY )
			break;
		if( sc->curValue > sc->minValue )
		{
			sc->curValue -= sc->range;
			if( sc->curValue < sc->minValue )
				sc->curValue = sc->minValue;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		if( sc->generic.flags & QMF_MOUSEONLY )
			break;

		if( sc->curValue < sc->maxValue )
		{
			sc->curValue += sc->range;
			if( sc->curValue > sc->maxValue )
				sc->curValue = sc->maxValue;
			sound = uiSoundMove;
		}
		else sound = uiSoundBuzz;
		break;
	}

	if( sound && ( sc->generic.flags & QMF_SILENT ))
		sound = uiSoundNull;

	if( sound && sc->generic.callback )
	{
		if( sound != uiSoundBuzz )
			sc->generic.callback( sc, QM_CHANGED );
	}
	return sound;
}

/*
=================
UI_SpinControl_Draw
=================
*/
void UI_SpinControl_Draw( menuSpinControl_s *sc )
{
	int	justify;
	bool	shadow;
	int	x, y, w, h;
	int	arrowWidth, arrowHeight, leftX, leftY, rightX, rightY;
	bool	leftFocus, rightFocus;
	
	if( sc->generic.flags & QMF_LEFT_JUSTIFY )
		justify = 0;
	else if( sc->generic.flags & QMF_CENTER_JUSTIFY )
		justify = 1;
	else if( sc->generic.flags & QMF_RIGHT_JUSTIFY )
		justify = 2;

	shadow = (sc->generic.flags & QMF_DROPSHADOW);

	// calculate size and position for the arrows
	arrowWidth = sc->generic.height;
	arrowHeight = sc->generic.height;

	leftX = sc->generic.x;
	leftY = sc->generic.y;
	rightX = sc->generic.x + (sc->generic.width - arrowWidth);
	rightY = sc->generic.y;

	// get size and position for the center box
	w = sc->generic.width2;
	h = sc->generic.height2;
	x = sc->generic.x2;
	y = sc->generic.y2;

	UI_DrawPic( x, y, w, h, uiColorWhite, sc->background );

	if( sc->generic.flags & QMF_GRAYED )
	{
		UI_DrawString( x, y, w, h, sc->generic.name, uiColorDkGrey, true, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, uiColorDkGrey, sc->leftArrow );
		UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, uiColorDkGrey, sc->rightArrow );
		return; // grayed
	}

	if((menuCommon_s *)sc != (menuCommon_s *)UI_ItemAtCursor(sc->generic.parent ))
	{
		UI_DrawString(x, y, w, h, sc->generic.name, sc->generic.color, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic(leftX, leftY, arrowWidth, arrowHeight, sc->generic.color, sc->leftArrow);
		UI_DrawPic(rightX, rightY, arrowWidth, arrowHeight, sc->generic.color, sc->rightArrow);
		return;		// No focus
	}

	// see which arrow has the mouse focus
	leftFocus = UI_CursorInRect( leftX, leftY, arrowWidth, arrowHeight );
	rightFocus = UI_CursorInRect( rightX, rightY, arrowWidth, arrowHeight );

	if( !( sc->generic.flags & QMF_FOCUSBEHIND ))
	{
		UI_DrawString( x, y, w, h, sc->generic.name, sc->generic.color, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, sc->generic.color, sc->leftArrow );
		UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, sc->generic.color, sc->rightArrow );
	}

	if( sc->generic.flags & QMF_HIGHLIGHTIFFOCUS )
	{
		UI_DrawString( x, y, w, h, sc->generic.name, sc->generic.focusColor, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, (leftFocus) ? sc->generic.focusColor : sc->generic.color, (leftFocus) ? sc->leftArrowFocus : sc->leftArrow );
		UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, (rightFocus) ? sc->generic.focusColor : sc->generic.color, (rightFocus) ? sc->rightArrowFocus : sc->rightArrow );
	}
	else if( sc->generic.flags & QMF_PULSEIFFOCUS )
	{
		rgba_t	color;

		*(uint *)color = *(uint *)sc->generic.color;
		color[3] = 255 * (0.5 + 0.5 * sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

		UI_DrawString( x, y, w, h, sc->generic.name, color, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, (leftFocus) ? color : sc->generic.color, (leftFocus) ? sc->leftArrowFocus : sc->leftArrow );
		UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, (rightFocus) ? color : sc->generic.color, (rightFocus) ? sc->rightArrowFocus : sc->rightArrow );
	}
	else if( sc->generic.flags & QMF_BLINKIFFOCUS )
	{
		if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
		{
			UI_DrawString( x, y, w, h, sc->generic.name, sc->generic.focusColor, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
			UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, (leftFocus) ? sc->generic.focusColor : sc->generic.color, (leftFocus) ? sc->leftArrowFocus : sc->leftArrow );
			UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, (rightFocus) ? sc->generic.focusColor : sc->generic.color, (rightFocus) ? sc->rightArrowFocus : sc->rightArrow );
		}
	}

	if( sc->generic.flags & QMF_FOCUSBEHIND )
	{
		UI_DrawString( x, y, w, h, sc->generic.name, sc->generic.color, false, sc->generic.charWidth, sc->generic.charHeight, justify, shadow );
		UI_DrawPic( leftX, leftY, arrowWidth, arrowHeight, sc->generic.color, sc->leftArrow );
		UI_DrawPic( rightX, rightY, arrowWidth, arrowHeight, sc->generic.color, sc->rightArrow );
	}
}

/*
=================
UI_Field_Init
=================
*/
void UI_Field_Init( menuField_s *f )
{
	if( !f->generic.name ) f->generic.name = "";

	if( f->generic.flags & QMF_BIGFONT )
	{
		f->generic.charWidth = UI_BIG_CHAR_WIDTH;
		f->generic.charHeight = UI_BIG_CHAR_HEIGHT;
	}
	else if( f->generic.flags & QMF_SMALLFONT )
	{
		f->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		f->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}
	else
	{
		if( f->generic.charWidth < 1 ) f->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		if( f->generic.charHeight < 1 ) f->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}

	UI_ScaleCoords( NULL, NULL, &f->generic.charWidth, &f->generic.charHeight );

	if( !(f->generic.flags & (QMF_LEFT_JUSTIFY|QMF_CENTER_JUSTIFY|QMF_RIGHT_JUSTIFY)))
		f->generic.flags |= QMF_LEFT_JUSTIFY;

	if( !f->generic.color ) f->generic.color = uiColorWhite;
	if( !f->generic.focusColor ) f->generic.focusColor = uiColorLtGrey;
	if( !f->background ) f->background = UI_BACKGROUNDBOX;

	f->maxLenght++;
	if( f->maxLenght <= 1 || f->maxLenght >= UI_MAX_FIELD_LINE )
		f->maxLenght = UI_MAX_FIELD_LINE - 1;

	UI_ScaleCoords( &f->generic.x, &f->generic.y, &f->generic.width, &f->generic.height );

	// calculate number of visible characters
	f->visibleLength = (f->generic.width / f->generic.charWidth) - 2;

	f->length = com.strlen( f->buffer );
	f->cursor = f->length;
}

/*
=================
UI_Field_Key
=================
*/
const char *UI_Field_Key( menuField_s *f, int key )
{
	char	*str, *s;
	int	i;

	switch( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_STAR:
		key = '*';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	case K_KP_ENTER:
		key = K_ENTER;
		break;
	}

	// clipboard paste
	if( key == K_INS && Key_IsDown( K_SHIFT ))
	{
		str = Sys_GetClipboardData();
		if( !str ) return 0;

		// insert at the current position
		s = str;
		while( *s )
		{
			if( f->length + 1 == f->maxLenght )
				break; // all full

			if( *s < 32 || *s > 126 )
			{
				s++;
				continue;
			}

			if( f->generic.flags & QMF_NUMBERSONLY )
			{
				if( *s < '0' || *s > '9' )
				{
					s++;
					continue;
				}
			}

			for( i = f->length + 1; i > f->cursor; i-- )
				f->buffer[i] = f->buffer[i-1];

			if( f->generic.flags & QMF_LOWERCASE )
				f->buffer[f->cursor++] = com.tolower( *s++ );
			else if( f->generic.flags & QMF_UPPERCASE )
				f->buffer[f->cursor++] = com.toupper( *s++ );
			else f->buffer[f->cursor++] = *s++;
			f->length++;
		}
		Mem_Free( str );

		if( f->generic.callback )
			f->generic.callback( f, QM_CHANGED );
		return 0;
	}

	// previous character
	if( key == K_LEFTARROW )
	{
		if( f->cursor )
			f->cursor--;

		return 0;
	}

	// Next character
	if (key == K_RIGHTARROW){
		if (f->cursor < f->length)
			f->cursor++;
		return 0;
	}

	// first character
	if( key == K_HOME )
	{
		f->cursor = 0;
		return 0;
	}

	// last character
	if( key == K_END )
	{
		f->cursor = f->length;
		return 0;
	}

	if( key == K_BACKSPACE )
	{
		// delete previous character
		if( !f->cursor )
			return 0;

		f->cursor--;
		for( i = f->cursor; i <= f->length; i++ )
			f->buffer[i] = f->buffer[i+1];
		f->buffer[f->length--] = 0;
	}
	else if( key == K_DEL )
	{	
		// delete next character
		if( f->cursor == f->length )
			return 0;

		for( i = f->cursor; i <= f->length; i++ )
			f->buffer[i] = f->buffer[i+1];
		f->buffer[f->length--] = 0;
	}
	else
	{
		// normal typing
		if( key < 32 || key > 126 )
			return 0;	// non printable

		if( f->length + 1 == f->maxLenght )
			return 0;	// all full

		if( f->generic.flags & QMF_NUMBERSONLY )
		{
			if( key < '0' || key > '9' )
				return 0;
		}

		if( f->generic.flags & QMF_LOWERCASE )
			key = com.tolower( key );
		else if( f->generic.flags & QMF_UPPERCASE )
			key = com.toupper( key );

		for( i = f->length + 1; i < f->cursor; i-- )
			f->buffer[i] = f->buffer[i-1];

		f->buffer[f->cursor++] = key;
		f->length++;
	}

	if( f->generic.callback )
		f->generic.callback( f, QM_CHANGED );
	return 0;
}

/*
=================
UI_Field_Draw
=================
*/
void UI_Field_Draw( menuField_s *f )
{
	int	justify;
	bool	shadow;
	char	*text = f->buffer;
	int	scroll = 0, width = f->visibleLength - 2;
	int	cursor, x;

	if( f->generic.flags & QMF_LEFT_JUSTIFY )
		justify = 0;
	else if( f->generic.flags & QMF_CENTER_JUSTIFY )
		justify = 1;
	else if( f->generic.flags & QMF_RIGHT_JUSTIFY )
		justify = 2;

	shadow = (f->generic.flags & QMF_DROPSHADOW);

	// prestep if horizontally scrolling
	while(com.cstrlen( text+scroll ) + 2 > f->visibleLength )
		scroll++;
	
	text += scroll;

	// find cursor position
	if( scroll ) cursor = width - (com.cstrlen( f->buffer ) - f->cursor );
	else cursor = f->cursor;

	cursor -= (com.strlen( f->buffer ) - com.cstrlen( f->buffer ));
	if( cursor > width ) cursor = width;
	if( cursor < 0 ) cursor = 0;

	if( justify == 0 ) x = f->generic.x;
	else if( justify == 1 ) x = f->generic.x + ((f->generic.width - (com.cstrlen( text ) * f->generic.charWidth)) / 2);
	else if( justify == 2 ) x = f->generic.x + (f->generic.width - (com.cstrlen( text ) * f->generic.charWidth));

	UI_DrawPic( f->generic.x, f->generic.y, f->generic.width, f->generic.height, uiColorWhite, f->background );

	if( f->generic.flags & QMF_GRAYED )
	{
		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, uiColorDkGrey, true, f->generic.charWidth, f->generic.charHeight, justify, shadow );
		return; // grayed
	}

	if((menuCommon_s *)f != (menuCommon_s *)UI_ItemAtCursor( f->generic.parent ))
	{
		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, f->generic.color, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );
		return; // no focus
	}

	if( !( f->generic.flags & QMF_FOCUSBEHIND ))
	{
		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, f->generic.color, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );

		if(( uiStatic.realTime & 499 ) < 250 )
			UI_DrawString( x + (cursor*f->generic.charWidth), f->generic.y, f->generic.charWidth, f->generic.height, "_", f->generic.color, true, f->generic.charWidth, f->generic.charHeight, 0, shadow );
	}

	if( f->generic.flags & QMF_HIGHLIGHTIFFOCUS )
	{
		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, f->generic.focusColor, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );

		if(( uiStatic.realTime & 499 ) < 250 )
			UI_DrawString( x + (cursor*f->generic.charWidth), f->generic.y, f->generic.charWidth, f->generic.height, "_", f->generic.focusColor, true, f->generic.charWidth, f->generic.charHeight, 0, shadow );
	}
	else if( f->generic.flags & QMF_PULSEIFFOCUS )
	{
		rgba_t	color;

		*(uint *)color = *(uint *)f->generic.color;
		color[3] = 255 * (0.5f + 0.5f * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, color, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );

		if(( uiStatic.realTime & 499 ) < 250 )
			UI_DrawString( x + (cursor*f->generic.charWidth), f->generic.y, f->generic.charWidth, f->generic.height, "_", color, true, f->generic.charWidth, f->generic.charHeight, 0, shadow );
	}
	else if( f->generic.flags & QMF_BLINKIFFOCUS )
	{
		if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
		{
			UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, f->generic.focusColor, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );

			if(( uiStatic.realTime & 499 ) < 250 )
				UI_DrawString( x + (cursor*f->generic.charWidth), f->generic.y, f->generic.charWidth, f->generic.height, "_", f->generic.focusColor, true, f->generic.charWidth, f->generic.charHeight, 0, shadow );
		}
	}

	if( f->generic.flags & QMF_FOCUSBEHIND )
	{
		UI_DrawString( f->generic.x, f->generic.y, f->generic.width, f->generic.height, text, f->generic.color, false, f->generic.charWidth, f->generic.charHeight, justify, shadow );

		if(( uiStatic.realTime & 499 ) < 250 )
			UI_DrawString( x + (cursor*f->generic.charWidth), f->generic.y, f->generic.charWidth, f->generic.height, "_", f->generic.color, true, f->generic.charWidth, f->generic.charHeight, 0, shadow );
	}
}

/*
=================
UI_Action_Init
=================
*/
void UI_Action_Init( menuAction_s *a )
{
	if( !a->generic.name ) a->generic.name = ""; // this is also the text displayed

	if( a->generic.flags & QMF_BIGFONT )
	{
		a->generic.charWidth = UI_BIG_CHAR_WIDTH;
		a->generic.charHeight = UI_BIG_CHAR_HEIGHT;
	}
	else if( a->generic.flags & QMF_SMALLFONT )
	{
		a->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		a->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}
	else
	{
		if( a->generic.charWidth < 1 ) a->generic.charWidth = UI_SMALL_CHAR_WIDTH;
		if( a->generic.charHeight < 1 ) a->generic.charHeight = UI_SMALL_CHAR_HEIGHT;
	}

	UI_ScaleCoords( NULL, NULL, &a->generic.charWidth, &a->generic.charHeight );

	if(!( a->generic.flags & (QMF_LEFT_JUSTIFY|QMF_CENTER_JUSTIFY|QMF_RIGHT_JUSTIFY)))
		a->generic.flags |= QMF_LEFT_JUSTIFY;

	if( !a->generic.color ) a->generic.color = uiColorWhite;
	if( !a->generic.focusColor ) a->generic.focusColor = uiColorLtGrey;
	if( !a->background ) a->background = UI_BACKGROUNDBOX;

	UI_ScaleCoords( &a->generic.x, &a->generic.y, &a->generic.width, &a->generic.height );
}

/*
=================
UI_Action_Key
=================
*/
const char *UI_Action_Key( menuAction_s *a, int key )
{
	const char	*sound = 0;

	switch( key )
	{
	case K_MOUSE1:
		if(!( a->generic.flags & QMF_HASMOUSEFOCUS ))
			break;
		sound = uiSoundMove;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if( a->generic.flags & QMF_MOUSEONLY )
			break;
		sound = uiSoundMove;
		break;
	}

	if( sound && ( a->generic.flags & QMF_SILENT ))
		sound = uiSoundNull;

	if( sound && a->generic.callback )
		a->generic.callback( a, QM_ACTIVATED );
	return sound;
}

/*
=================
UI_Action_Draw
=================
*/
void UI_Action_Draw( menuAction_s *a )
{
	int	justify;
	bool	shadow;

	if( a->generic.flags & QMF_LEFT_JUSTIFY )
		justify = 0;
	else if( a->generic.flags & QMF_CENTER_JUSTIFY )
		justify = 1;
	else if( a->generic.flags & QMF_RIGHT_JUSTIFY )
		justify = 2;

	shadow = (a->generic.flags & QMF_DROPSHADOW);

	UI_DrawPic( a->generic.x, a->generic.y, a->generic.width, a->generic.height, uiColorWhite, a->background );

	if( a->generic.flags & QMF_GRAYED )
	{
		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, uiColorDkGrey, true, a->generic.charWidth, a->generic.charHeight, justify, shadow );
		return; // grayed
	}

	if((menuCommon_s *)a != (menuCommon_s *)UI_ItemAtCursor( a->generic.parent ))
	{
		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, a->generic.color, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );
		return;		// No focus
	}

	if(!( a->generic.flags & QMF_FOCUSBEHIND ))
		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, a->generic.color, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );

	if( a->generic.flags & QMF_HIGHLIGHTIFFOCUS )
		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, a->generic.focusColor, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );
	else if( a->generic.flags & QMF_PULSEIFFOCUS )
	{
		rgba_t	color;

		*(uint *)color = *(uint *)a->generic.color;
		color[3] = 255 * (0.5 + 0.5 * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, color, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );
	}
	else if( a->generic.flags & QMF_BLINKIFFOCUS )
	{
		if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
			UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, a->generic.focusColor, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );
	}

	if( a->generic.flags & QMF_FOCUSBEHIND )
		UI_DrawString( a->generic.x, a->generic.y, a->generic.width, a->generic.height, a->generic.name, a->generic.color, false, a->generic.charWidth, a->generic.charHeight, justify, shadow );
}

/*
=================
UI_Bitmap_Init
=================
*/
void UI_Bitmap_Init( menuBitmap_s *b )
{
	if( !b->generic.name ) b->generic.name = "";
	if( !b->focusPic ) b->focusPic = b->pic;
	if( !b->generic.color ) b->generic.color = uiColorWhite;
	if( !b->generic.focusColor ) b->generic.focusColor = uiColorLtGrey;

	UI_ScaleCoords( &b->generic.x, &b->generic.y, &b->generic.width, &b->generic.height );
}

/*
=================
UI_Bitmap_Key
=================
*/
const char *UI_Bitmap_Key( menuBitmap_s *b, int key )
{
	const char	*sound = 0;

	switch( key )
	{
	case K_MOUSE1:
		if(!( b->generic.flags & QMF_HASMOUSEFOCUS ))
			break;
		sound = uiSoundMove;
		break;
	case K_ENTER:
	case K_KP_ENTER:
		if( b->generic.flags & QMF_MOUSEONLY )
			break;
		sound = uiSoundMove;
		break;
	}
	if( sound && ( b->generic.flags & QMF_SILENT ))
		sound = uiSoundNull;

	if( sound && b->generic.callback )
		b->generic.callback( b, QM_ACTIVATED );
	return sound;
}

/*
=================
UI_Bitmap_Draw
=================
*/
void UI_Bitmap_Draw( menuBitmap_s *b )
{
	if( b->generic.flags & QMF_GRAYED )
	{
		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, uiColorDkGrey, b->pic );
		return; // grayed
	}

	if((menuCommon_s *)b != (menuCommon_s *)UI_ItemAtCursor( b->generic.parent ))
	{
		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, b->generic.color, b->pic );
		return; // no focus
	}

	if(!( b->generic.flags & QMF_FOCUSBEHIND ))
		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, b->generic.color, b->pic );

	if( b->generic.flags & QMF_HIGHLIGHTIFFOCUS )
		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, b->generic.focusColor, b->focusPic );
	else if( b->generic.flags & QMF_PULSEIFFOCUS )
	{
		rgba_t	color;

		*(uint *)color = *(uint *)b->generic.color;
		color[3] = 255 * (0.5 + 0.5 * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR));

		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, color, b->focusPic );
	}
	else if( b->generic.flags & QMF_BLINKIFFOCUS )
	{
		if(( uiStatic.realTime & UI_BLINK_MASK ) < UI_BLINK_TIME )
			UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, b->generic.focusColor, b->focusPic );
	}

	if( b->generic.flags & QMF_FOCUSBEHIND )
		UI_DrawPic( b->generic.x, b->generic.y, b->generic.width, b->generic.height, b->generic.color, b->pic );
}