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


// ui_menu.c -- main menu interface

#include "common.h"
#include "ui_local.h"
#include "client.h"
#include "input.h"

cvar_t	*ui_mainfont;
cvar_t	*ui_namefont;
cvar_t	*ui_precache;
cvar_t	*ui_sensitivity;

uiStatic_t	uiStatic;

string		uiEmptyString;
const char	*uiSoundIn	= "common/menu1.wav";
const char	*uiSoundMove	= "common/menu2.wav";
const char	*uiSoundOut	= "common/menu3.wav";
const char	*uiSoundBuzz	= "common/menu4.wav";
const char	*uiSoundGlow	= "common/menu5.wav";
const char	*uiSoundNull	= "";

rgba_t		uiColorHelp	= {255, 255, 255, 255};	// hint letters color
rgba_t		uiPromptBgColor	= {64,  64,  64,  255};	// dialog background color
rgba_t		uiPromptTextColor	= {255, 160,  0,  255};	// dialog or button letters color
rgba_t		uiPromptFocusColor	= {255, 255,  0,  255};	// dialog or button focus letters color
rgba_t		uiInputTextColor	= {192, 192, 192, 255};
rgba_t		uiInputBgColor	= {64,  64,  64,  255};	// field, scrollist, checkbox background color
rgba_t		uiInputFgColor	= {85,  85,  85,  255};	// field, scrollist, checkbox foreground color
rgba_t		uiColorWhite	= {255, 255, 255, 255};	// useful for bitmaps
rgba_t		uiColorDkGrey	= { 64,  64,  64, 255};	// shadow and grayed items
rgba_t		uiColorBlack	= {  0,   0,   0, 255};	// some controls background


/*
=================
UI_ScaleCoords

Any parameter can be NULL if you don't want it
=================
*/
void UI_ScaleCoords( int *x, int *y, int *w, int *h )
{
	if( x ) *x *= uiStatic.scaleX;
	if( y ) *y *= uiStatic.scaleY;
	if( w ) *w *= uiStatic.scaleX;
	if( h ) *h *= uiStatic.scaleY;
}

/*
=================
UI_CursorInRect
=================
*/
bool UI_CursorInRect( int x, int y, int w, int h )
{
	if( uiStatic.cursorX < x )
		return false;
	if( uiStatic.cursorX > x+w )
		return false;
	if( uiStatic.cursorY < y )
		return false;
	if( uiStatic.cursorY > y+h )
		return false;
	return true;
}

/*
=================
UI_DrawPic
=================
*/
void UI_DrawPic( int x, int y, int w, int h, const rgba_t color, const char *pic )
{
	shader_t	shader;

	if( !re || !pic || !pic[0] ) return;

	shader = re->RegisterShader( pic, SHADER_NOMIP );

	re->SetColor( color );
	re->DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
	re->SetColor( NULL );
}

/*
=================
UI_FillRect
=================
*/
void UI_FillRect( int x, int y, int w, int h, const rgba_t color )
{
	shader_t	shader;

	if( !re ) return;

	shader = re->RegisterShader( UI_WHITE_SHADER, SHADER_FONT );

	re->SetColor( color );
	re->DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
	re->SetColor( NULL );
}

/*
=================
UI_DrawRectangleExt
=================
*/
void UI_DrawRectangleExt( int in_x, int in_y, int in_w, int in_h, const rgba_t color, int outlineWidth )
{
	int	x, y, w, h;

	x = in_x - outlineWidth;
	y = in_y - outlineWidth;
	w = outlineWidth;
	h = in_h + outlineWidth + outlineWidth;

	// draw left
	UI_FillRect( x, y, w, h, color );

	x = in_x + in_w;
	y = in_y - outlineWidth;
	w = outlineWidth;
	h = in_h + outlineWidth + outlineWidth;

	// draw right
	UI_FillRect( x, y, w, h, color );

	x = in_x;
	y = in_y - outlineWidth;
	w = in_w;
	h = outlineWidth;

	// draw top
	UI_FillRect( x, y, w, h, color );

	// draw bottom
	x = in_x;
	y = in_y + in_h;
	w = in_w;
	h = outlineWidth;

	UI_FillRect( x, y, w, h, color );
}

/*
=================
UI_DrawString
=================
*/
void UI_DrawStringExt( int x, int y, int w, int h, const char *string, const rgba_t color, bool forceColor, int charW, int charH, int justify, bool shadow, shader_t font )
{
	rgba_t	modulate, shadowModulate;
	char	line[1024], *l;
	int	xx, yy, ofsX, ofsY, len, ch;
	float	col, row, size;

	if( !re || !string || !string[0] )
		return;

	// vertically centered
	if( !com.strchr( string, '\n' ))
		y = y + ((h - charH) / 2 );

	if( shadow )
	{
		MakeRGBA( shadowModulate, 0, 0, 0, color[3] );

		ofsX = charW / 8;
		ofsY = charH / 8;
	}

	*(uint *)modulate = *(uint *)color;

	yy = y;
	while( *string )
	{
		// get a line of text
		len = 0;
		while( *string )
		{
			if( *string == '\n' )
			{
				string++;
				break;
			}

			line[len++] = *string++;
			if( len == sizeof( line ) - 1 )
				break;
		}
		line[len] = 0;

		// align the text as appropriate
		if( justify == 0 ) xx = x;
		if( justify == 1 ) xx = x + ((w - (com.cstrlen( line ) * charW )) / 2);
		if( justify == 2 ) xx = x + (w - (com.cstrlen( line ) * charW ));

		// draw it
		l = line;
		while( *l )
		{
			if( IsColorString( l ))
			{
				if( !forceColor )
				{
					*(uint *)modulate = *(uint *)g_color_table[ColorIndex(*(l+1))];
					modulate[3] = color[3];
				}

				l += 2;
				continue;
			}

			ch = *l++;

			ch &= 255;
			if( ch != ' ' )
			{
				col = (ch & 15) * 0.0625 + (0.5f / 256.0f);
				row = (ch >> 4) * 0.0625 + (0.5f / 256.0f);
				size = 0.0625f - (1.0f / 256.0f);

				if( shadow )
				{
					re->SetColor( shadowModulate );
					re->DrawStretchPic( xx + ofsX, yy + ofsY, charW, charH, col, row, col + size, row + size, font );
                                        }
				re->SetColor( modulate );
				re->DrawStretchPic( xx, yy, charW, charH, col, row, col + size, row + size, font );
			}
          		xx += charW;
		}
          	yy += charH;
	}
	re->SetColor( NULL );
}

/*
=================
UI_DrawMouseCursor
=================
*/
void UI_DrawMouseCursor( void )
{
	menuCommon_s	*item;
	shader_t		shader = -1;
	int		w = UI_CURSOR_SIZE;
	int		h = UI_CURSOR_SIZE;
	int		i;

	if( !re || uiStatic.hideCursor ) return;
	UI_ScaleCoords( NULL, NULL, &w, &h );

	for( i = 0; i < uiStatic.menuActive->numItems; i++ )
	{
		item = (menuCommon_s *)uiStatic.menuActive->items[i];

		if( item->flags & (QMF_INACTIVE|QMF_HIDDEN))
			continue;

		if( !UI_CursorInRect( item->x, item->y, item->width, item->height ))
			continue;

		if( item->flags & QMF_GRAYED )
		{
			shader = re->RegisterShader( UI_CURSOR_DISABLED, SHADER_NOMIP );
		}
		else
		{
			if( item->type == QMTYPE_FIELD )
				shader = re->RegisterShader( UI_CURSOR_TYPING, SHADER_NOMIP );
		}
		break;
	}

	if( shader == -1 ) shader = re->RegisterShader( UI_CURSOR_NORMAL, SHADER_NOMIP );
	re->DrawStretchPic( uiStatic.cursorX, uiStatic.cursorY, w, h, 0, 0, 1, 1, shader );
}

/*
=================
UI_StartSound
=================
*/
void UI_StartSound( const char *sound )
{
	S_StartLocalSound( sound, 1.0f, 100.0f, vec3_origin );
}

/*
=================
UI_BuildPathExt

helper to search dlls
=================
*/
void UI_BuildPathExt( const char *dllname, char *fullpath, size_t size )
{
	string	name;

	if( !dllname || !fullpath || size <= 0 ) return;

	// only libraries with extension .dll are valid
	com.strncpy( name, dllname, sizeof( string ));
	FS_FileBase( name, name );

	// game path (Xash3D/game/bin/)
	com.snprintf( fullpath, size, "bin/%s.dll", name );
	if( FS_FileExists( fullpath )) return; // found

	// absoulte path (Xash3D/bin/)
	com.snprintf( fullpath, size, "%s.dll", name );	
	if( FS_FileExists( fullpath )) return; // found

	fullpath[0] = 0;
}

// =====================================================================


/*
=================
UI_AddItem
=================
*/
void UI_AddItem( menuFramework_s *menu, void *item )
{
	menuCommon_s	*generic = (menuCommon_s *)item;

	if( menu->numItems >= UI_MAX_MENUITEMS )
		Host_Error( "UI_AddItem: UI_MAX_MENUITEMS limit exceeded\n" );

	menu->items[menu->numItems] = item;
	((menuCommon_s *)menu->items[menu->numItems])->parent = menu;
	((menuCommon_s *)menu->items[menu->numItems])->flags &= ~QMF_HASMOUSEFOCUS;
	menu->numItems++;

	switch( generic->type )
	{
	case QMTYPE_SCROLLLIST:
		UI_ScrollList_Init((menuScrollList_s *)item );
		break;
	case QMTYPE_SPINCONTROL:
		UI_SpinControl_Init((menuSpinControl_s *)item );
		break;
	case QMTYPE_CHECKBOX:
		UI_CheckBox_Init((menuCheckBox_s *)item );
		break;
	case QMTYPE_SLIDER:
		UI_Slider_Init((menuSlider_s *)item );
		break;
	case QMTYPE_FIELD:
		UI_Field_Init((menuField_s *)item );
		break;
	case QMTYPE_ACTION:
		UI_Action_Init((menuAction_s *)item );
		break;
	case QMTYPE_BITMAP:
		UI_Bitmap_Init((menuBitmap_s *)item );
		break;
	default:
		Host_Error( "UI_AddItem: unknown item type (%i)\n", generic->type );
	}
}

/*
=================
UI_CursorMoved
=================
*/
void UI_CursorMoved( menuFramework_s *menu )
{
	void (*callback)( void *self, int event );

	if( menu->cursor == menu->cursorPrev )
		return;

	if( menu->cursorPrev >= 0 && menu->cursorPrev < menu->numItems )
	{
		callback = ((menuCommon_s *)menu->items[menu->cursorPrev])->callback;
		if( callback ) callback( menu->items[menu->cursorPrev], QM_LOSTFOCUS );
	}

	if( menu->cursor >= 0 && menu->cursor < menu->numItems )
	{
		callback = ((menuCommon_s *)menu->items[menu->cursor])->callback;
		if( callback ) callback( menu->items[menu->cursor], QM_GOTFOCUS );
	}
}

/*
=================
UI_SetCursor
=================
*/
void UI_SetCursor( menuFramework_s *menu, int cursor )
{
	if(((menuCommon_s *)(menu->items[cursor]))->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN))
		return;

	menu->cursorPrev = menu->cursor;
	menu->cursor = cursor;

	UI_CursorMoved( menu );
}

/*
=================
UI_SetCursorToItem
=================
*/
void UI_SetCursorToItem( menuFramework_s *menu, void *item )
{
	int	i;

	for( i = 0; i < menu->numItems; i++ )
	{
		if( menu->items[i] == item )
		{
			UI_SetCursor( menu, i );
			return;
		}
	}
}

/*
=================
UI_ItemAtCursor
=================
*/
void *UI_ItemAtCursor( menuFramework_s *menu )
{
	if( menu->cursor < 0 || menu->cursor >= menu->numItems )
		return 0;

	// inactive items can't be has focus
	if( ((menuCommon_s *)menu->items[menu->cursor])->flags & QMF_INACTIVE )
		return 0;

	return menu->items[menu->cursor];
}

/*
=================
UI_AdjustCursor

This functiont takes the given menu, the direction, and attempts to
adjust the menu's cursor so that it's at the next available slot
=================
*/
void UI_AdjustCursor( menuFramework_s *menu, int dir )
{
	menuCommon_s	*item;
	bool		wrapped = false;
wrap:
	while( menu->cursor >= 0 && menu->cursor < menu->numItems )
	{
		item = (menuCommon_s *)menu->items[menu->cursor];
		if( item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY))
			menu->cursor += dir;
		else break;
	}

	if( dir == 1 )
	{
		if( menu->cursor >= menu->numItems )
		{
			if( wrapped )
			{
				menu->cursor = menu->cursorPrev;
				return;
			}

			menu->cursor = 0;
			wrapped = true;
			goto wrap;
		}
	}
	else if( dir == -1 )
	{
		if( menu->cursor < 0 )
		{
			if( wrapped )
			{
				menu->cursor = menu->cursorPrev;
				return;
			}
			menu->cursor = menu->numItems - 1;
			wrapped = true;
			goto wrap;
		}
	}
}

/*
=================
UI_DrawMenu
=================
*/
void UI_DrawMenu( menuFramework_s *menu )
{
	static long	statusFadeTime;
	static menuCommon_s	*lastItem;
	rgba_t		color;
	menuCommon_s	*item;
	int		i;

	color[0] = uiColorHelp[0];
	color[1] = uiColorHelp[1];
	color[2] = uiColorHelp[2];
	color[3] = 255;

	// draw contents
	for( i = 0; i < menu->numItems; i++ )
	{
		item = (menuCommon_s *)menu->items[i];

		if( item->flags & QMF_HIDDEN )
			continue;

		if( item->ownerdraw )
		{
			// total subclassing, owner draws everything
			item->ownerdraw( item );
			continue;
		}

		switch( item->type )
		{
		case QMTYPE_SCROLLLIST:
			UI_ScrollList_Draw((menuScrollList_s *)item );
			break;
		case QMTYPE_SPINCONTROL:
			UI_SpinControl_Draw((menuSpinControl_s *)item );
			break;
		case QMTYPE_CHECKBOX:
			UI_CheckBox_Draw((menuCheckBox_s *)item );
			break;
		case QMTYPE_SLIDER:
			UI_Slider_Draw((menuSlider_s *)item );
			break;
		case QMTYPE_FIELD:
			UI_Field_Draw((menuField_s *)item );
			break;
		case QMTYPE_ACTION:
			UI_Action_Draw((menuAction_s *)item );
			break;
		case QMTYPE_BITMAP:
			UI_Bitmap_Draw((menuBitmap_s *)item );
			break;
		}
	}

	// draw status bar
	item = UI_ItemAtCursor( menu );
	if( item != lastItem )
	{
		statusFadeTime = uiStatic.realTime;
		lastItem = item;
	}

	if( item && ( item->flags & QMF_HASMOUSEFOCUS && !( item->flags & QMF_NOTIFY )) && ( item->statusText != NULL ))
	{
		// fade it in, but wait a second
		color[3] = bound( 0.0, ((uiStatic.realTime - statusFadeTime) - 1000) * 0.001f, 1.0f ) * 255;

		UI_DrawStringExt( 0, 720 * uiStatic.scaleY, 1024 * uiStatic.scaleX, 28 * uiStatic.scaleY, item->statusText, color, true,
		UI_SMALL_CHAR_WIDTH * uiStatic.scaleX, UI_SMALL_CHAR_HEIGHT * uiStatic.scaleY, 1, true, cls.consoleFont );
	}
	else statusFadeTime = uiStatic.realTime;
}

/*
=================
UI_DefaultKey
=================
*/
const char *UI_DefaultKey( menuFramework_s *menu, int key, bool down )
{
	const char	*sound = NULL;
	menuCommon_s	*item;
	int		cursorPrev;

	// menu system key
	if( down && ( key == K_ESCAPE || key == K_MOUSE2 ))
	{
		UI_PopMenu();
		return uiSoundOut;
	}

	if( !menu || !menu->numItems )
		return 0;

	item = UI_ItemAtCursor( menu );
	if( item && !(item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
	{
		switch( item->type )
		{
		case QMTYPE_SCROLLLIST:
			sound = UI_ScrollList_Key((menuScrollList_s *)item, key, down );
			break;
		case QMTYPE_SPINCONTROL:
			sound = UI_SpinControl_Key((menuSpinControl_s *)item, key, down );
			break;
		case QMTYPE_CHECKBOX:
			sound = UI_CheckBox_Key((menuCheckBox_s *)item, key, down );
			break;
		case QMTYPE_SLIDER:
			sound = UI_Slider_Key((menuSlider_s *)item, key, down );
			break;
		case QMTYPE_FIELD:
			sound = UI_Field_Key((menuField_s *)item, key, down );
			break;
		case QMTYPE_ACTION:
			sound = UI_Action_Key((menuAction_s *)item, key, down );
			break;
		case QMTYPE_BITMAP:
			sound = UI_Bitmap_Key((menuBitmap_s *)item, key, down );
			break;
		}
		if( sound ) return sound; // key was handled
	}

	// system keys are always wait for keys down and never keys up
	if( !down ) return 0;

	// default handling
	switch( key )
	{
	case K_UPARROW:
	case K_KP_UPARROW:
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		cursorPrev = menu->cursor;
		menu->cursorPrev = menu->cursor;
		menu->cursor--;

		UI_AdjustCursor( menu, -1 );
		if( cursorPrev != menu->cursor )
		{
			UI_CursorMoved( menu );
			if( !(((menuCommon_s *)menu->items[menu->cursor])->flags & QMF_SILENT ))
				sound = uiSoundMove;
		}
		break;
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
	case K_TAB:
		cursorPrev = menu->cursor;
		menu->cursorPrev = menu->cursor;
		menu->cursor++;

		UI_AdjustCursor(menu, 1);
		if( cursorPrev != menu->cursor )
		{
			UI_CursorMoved(menu);
			if( !(((menuCommon_s *)menu->items[menu->cursor])->flags & QMF_SILENT ))
				sound = uiSoundMove;
		}
		break;
	case K_MOUSE1:
		if( item )
		{
			if((item->flags & QMF_HASMOUSEFOCUS) && !(item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
				return UI_ActivateItem( menu, item );
		}

		break;
	case K_ENTER:
	case K_KP_ENTER:
		if( item )
		{
			if( !(item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY)))
				return UI_ActivateItem( menu, item );
		}
		break;
	}
	return sound;
}		

/*
=================
UI_ActivateItem
=================
*/
const char *UI_ActivateItem( menuFramework_s *menu, menuCommon_s *item )
{
	if( item->callback )
	{
		item->callback( item, QM_ACTIVATED );

		if( !( item->flags & QMF_SILENT ))
			return uiSoundMove;
	}
	return 0;
}

/*
=================
UI_RefreshServerList
=================
*/
void UI_RefreshServerList( void )
{
	uiStatic.numServers = 0;
	Mem_Set( uiStatic.serverAddresses, 0, sizeof( uiStatic.serverAddresses ));
	Mem_Set( uiStatic.serverNames, 0, sizeof( uiStatic.serverNames ));

	Cbuf_ExecuteText( EXEC_APPEND, "localservers\n" );
}


// =====================================================================

/*
=================
UI_CloseMenu
=================
*/
void UI_CloseMenu( void )
{
	uiStatic.menuActive = NULL;
	uiStatic.menuDepth = 0;
	uiStatic.visible = false;

	Key_ClearStates();

	Key_SetKeyDest( key_game );
}

/*
=================
UI_PushMenu
=================
*/
void UI_PushMenu( menuFramework_s *menu )
{
	int		i;
	menuCommon_s	*item;

	// if this menu is already present, drop back to that level to avoid stacking menus by hotkeys
	for( i = 0; i < uiStatic.menuDepth; i++ )
	{
		if( uiStatic.menuStack[i] == menu )
		{
			uiStatic.menuDepth = i;
			break;
		}
	}

	if( i == uiStatic.menuDepth )
	{
		if( uiStatic.menuDepth >= UI_MAX_MENUDEPTH )
			Host_Error( "UI_PushMenu: menu stack overflow\n" );
		uiStatic.menuStack[uiStatic.menuDepth++] = menu;
	}

	uiStatic.menuActive = menu;
	uiStatic.firstDraw = true;
	uiStatic.enterSound = true;
	uiStatic.visible = true;

	Key_SetKeyDest( key_menu );

	menu->cursor = 0;
	menu->cursorPrev = 0;

	// force first available item to have focus
	for( i = 0; i < menu->numItems; i++ )
	{
		item = (menuCommon_s *)menu->items[i];

		if( item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN|QMF_MOUSEONLY))
			continue;

		menu->cursorPrev = -1;
		UI_SetCursor( menu, i );
		break;
	}
}

/*
=================
UI_PopMenu
=================
*/
void UI_PopMenu( void )
{
	UI_StartSound( uiSoundOut );

	uiStatic.menuDepth--;

	if( uiStatic.menuDepth < 0 )
		Host_Error( "UI_PopMenu: menu stack underflow\n" );

	if( uiStatic.menuDepth )
	{
		uiStatic.menuActive = uiStatic.menuStack[uiStatic.menuDepth-1];
		uiStatic.firstDraw = true;
	}
	else UI_CloseMenu();
}

// =====================================================================

/*
=================
UI_UpdateMenu
=================
*/
void UI_UpdateMenu( int realTime )
{
	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	uiStatic.realTime = realTime;

	if( uiStatic.firstDraw )
	{
		if( uiStatic.menuActive->activateFunc )
			uiStatic.menuActive->activateFunc();
	}

	// draw menu
	if( uiStatic.menuActive->drawFunc )
		uiStatic.menuActive->drawFunc();
	else UI_DrawMenu( uiStatic.menuActive );

	if( uiStatic.firstDraw )
	{
		UI_MouseMove( 0, 0 );
		uiStatic.firstDraw = false;
	}

	// draw cursor
	UI_DrawMouseCursor();

	// delay playing the enter sound until after the menu has been
	// drawn, to avoid delay while caching images
	if( uiStatic.enterSound )
	{
		UI_StartSound( uiSoundIn );
		uiStatic.enterSound = false;
	}
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent( int key, bool down )
{
	const char	*sound;

	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	if( uiStatic.menuActive->keyFunc )
		sound = uiStatic.menuActive->keyFunc( key, down );
	else sound = UI_DefaultKey( uiStatic.menuActive, key, down );

	if( !down ) return;
	if( sound && sound != uiSoundNull )
		UI_StartSound( sound );
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent( int key )
{
	menuFramework_s	*menu;
	menuCommon_s	*item;

	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	menu = uiStatic.menuActive;

	if( !menu || !menu->numItems )
		return;

	item = UI_ItemAtCursor( menu );
	if( item && !(item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN)))
	{
		switch( item->type )
		{
		case QMTYPE_FIELD:
			UI_Field_Char((menuField_s *)item, key );
			break;
		}
	}
}

/*
=================
UI_MouseMove
=================
*/
void UI_MouseMove( int x, int y )
{
	int		i;
	menuCommon_s	*item;

	if( !uiStatic.initialized )
		return;

	if( !uiStatic.visible )
		return;

	if( !uiStatic.menuActive )
		return;

	x *= ui_sensitivity->value;
	y *= ui_sensitivity->value;

	uiStatic.cursorX += x;
	uiStatic.cursorY += y;

	if( UI_CursorInRect( 1, 1, scr_width->integer - 1, scr_height->integer - 1 ))
		uiStatic.mouseInRect = true;
	else uiStatic.mouseInRect = false;

	uiStatic.cursorX = bound( 0, uiStatic.cursorX, scr_width->integer );
	uiStatic.cursorY = bound( 0, uiStatic.cursorY, scr_height->integer );

	// region test the active menu items
	for( i = 0; i < uiStatic.menuActive->numItems; i++ )
	{
		item = (menuCommon_s *)uiStatic.menuActive->items[i];

		if( item->flags & (QMF_GRAYED|QMF_INACTIVE|QMF_HIDDEN))
			continue;

		if( !UI_CursorInRect( item->x, item->y, item->width, item->height ))
			continue;

		// set focus to item at cursor
		if( uiStatic.menuActive->cursor != i )
		{
			UI_SetCursor( uiStatic.menuActive, i );
			((menuCommon_s *)(uiStatic.menuActive->items[uiStatic.menuActive->cursorPrev]))->flags &= ~QMF_HASMOUSEFOCUS;

			if (!(((menuCommon_s *)(uiStatic.menuActive->items[uiStatic.menuActive->cursor]))->flags & QMF_SILENT ))
				UI_StartSound( uiSoundMove );
		}

		((menuCommon_s *)(uiStatic.menuActive->items[uiStatic.menuActive->cursor]))->flags |= QMF_HASMOUSEFOCUS;
		return;
	}

	// out of any region
	if( uiStatic.menuActive->numItems )
	{
		((menuCommon_s *)(uiStatic.menuActive->items[uiStatic.menuActive->cursor]))->flags &= ~QMF_HASMOUSEFOCUS;

		// a mouse only item restores focus to the previous item
		if(((menuCommon_s *)(uiStatic.menuActive->items[uiStatic.menuActive->cursor]))->flags & QMF_MOUSEONLY )
		{
			if( uiStatic.menuActive->cursorPrev != -1 )
				uiStatic.menuActive->cursor = uiStatic.menuActive->cursorPrev;
		}
	}
}

/*
=================
UI_SetActiveMenu
=================
*/
void UI_SetActiveMenu( uiActiveMenu_t activeMenu )
{
	if( !uiStatic.initialized )
		return;

	// don't continue firing if we leave game
	Key_ClearStates();

	switch( activeMenu )
	{
	case UI_CLOSEMENU:
		UI_CloseMenu();
		break;
	case UI_MAINMENU:
		Key_SetKeyDest( key_menu );
		UI_Main_Menu();
		break;
	default:
		Host_Error( "UI_SetActiveMenu: wrong menu type (%i)\n", activeMenu );
	}
}

/*
=================
UI_AddServerToList
=================
*/
void UI_AddServerToList( netadr_t adr, const char *info )
{
	int	i;

	if( !uiStatic.initialized )
		return;

	if( uiStatic.numServers == UI_MAX_SERVERS )
		return;	// full

	// ignore if duplicated
	for( i = 0; i < uiStatic.numServers; i++ )
	{
		if( !com.stricmp( uiStatic.serverNames[i], info ))
			return;
	}

	// add it to the list
	uiStatic.updateServers = true; // info has been updated
	uiStatic.serverAddresses[uiStatic.numServers] = adr;
	com.strncpy( uiStatic.serverNames[uiStatic.numServers], info, sizeof( uiStatic.serverNames[uiStatic.numServers] ));
	uiStatic.numServers++;
}

/*
=================
UI_IsVisible

Some systems may need to know if it is visible or not
=================
*/
bool UI_IsVisible( void )
{
	if( !uiStatic.initialized )
		return false;
	return uiStatic.visible;
}

void UI_GetCursorPos( POINT *pos )
{
	if( !pos ) return;

	pos->x = uiStatic.cursorX;
	pos->y = uiStatic.cursorY;
}

void UI_SetCursorPos( int pos_x, int pos_y )
{
	uiStatic.cursorX = bound( 0, pos_x, scr_width->integer );
	uiStatic.cursorY = bound( 0, pos_y, scr_height->integer );
	uiStatic.mouseInRect = true;
}

void UI_ShowCursor( bool show )
{
	uiStatic.hideCursor = (show) ? false : true;
}

bool UI_MouseInRect( void )
{
	return uiStatic.mouseInRect;
}

/*
=================
UI_Precache
=================
*/
void UI_Precache( void )
{
	if( !uiStatic.initialized )
		return;

	if( !ui_precache->integer )
		return;

	S_RegisterSound( uiSoundIn );
	S_RegisterSound( uiSoundMove );
	S_RegisterSound( uiSoundOut );
	S_RegisterSound( uiSoundBuzz );

	if( re )
	{
		re->RegisterShader( UI_CURSOR_NORMAL, SHADER_NOMIP );
		re->RegisterShader( UI_CURSOR_DISABLED, SHADER_NOMIP );
		re->RegisterShader( UI_CURSOR_TYPING, SHADER_NOMIP );
		re->RegisterShader( UI_LEFTARROW, SHADER_NOMIP );
		re->RegisterShader( UI_LEFTARROWFOCUS, SHADER_NOMIP );
		re->RegisterShader( UI_RIGHTARROW, SHADER_NOMIP );
		re->RegisterShader( UI_RIGHTARROWFOCUS, SHADER_NOMIP );
		re->RegisterShader( UI_UPARROW, SHADER_NOMIP );
		re->RegisterShader( UI_UPARROWFOCUS, SHADER_NOMIP );
		re->RegisterShader( UI_DOWNARROW, SHADER_NOMIP );
		re->RegisterShader( UI_DOWNARROWFOCUS, SHADER_NOMIP );
	}

	if( ui_precache->integer == 1 )
		return;

	UI_Main_Precache();
	UI_NewGame_Precache();
	UI_LoadGame_Precache();
	UI_SaveGame_Precache();
	UI_SaveLoad_Precache();
	UI_MultiPlayer_Precache();
	UI_Options_Precache();
	UI_LanGame_Precache();
	UI_PlayerSetup_Precache();
	UI_Controls_Precache();
	UI_AdvControls_Precache();
	UI_GameOptions_Precache();
	UI_CreateGame_Precache();
	UI_PlayDemo_Precache();
	UI_RecDemo_Precache();
	UI_PlayRec_Precache();
	UI_Audio_Precache();
	UI_Video_Precache();
	UI_VidOptions_Precache();
	UI_VidModes_Precache();
	UI_CustomGame_Precache();
	UI_Credits_Precache();
}

/*
====================
UI_SetFont_f

menufont <fontname> <con>
====================
*/
void UI_SetFont_f( void )
{
	if( Cmd_Argc() < 2 )
	{
		Msg( "Usage: menufont <fontname> <console>\n" );
		return;
	}

	switch(Cmd_Argc( ))
	{
	case 2:
		Cvar_Set( "ui_mainfont", Cmd_Argv( 1 ));
		uiStatic.menuFont = re->RegisterShader( va( "gfx/fonts/%s", ui_mainfont->string ), SHADER_FONT );
		break;
	case 3:
		Cvar_Set( "ui_namefont", Cmd_Argv( 1 ));
		uiStatic.nameFont = re->RegisterShader( va( "gfx/fonts/%s", ui_namefont->string ), SHADER_FONT );
		break;
	default:
		Msg( "menufont: invalid arguments\n" );
		break;
	}
}

void UI_ParseColor( script_t *script, rgba_t outColor )
{
	int	i, color[4];

	for( i = 0; i < 4; i++ )
	{
		if( !Com_ReadLong( script, 0, &color[i] ))
			break; // bad declaration or missed alpha
	}

	if( i < 3 ) return; // bad color declaration

	outColor[0] = bound( 0, color[0], 255 );
	outColor[1] = bound( 0, color[1], 255 );
	outColor[2] = bound( 0, color[2], 255 );
	outColor[3] = (i == 4 ) ? bound( 0, color[3], 255 ) : 255;	
}

void UI_ApplyCustomColors( void )
{
	script_t	*script = NULL;
	token_t	token;

	script = Com_OpenScript( "scripts/colors.lst", NULL, 0 );
	if( !script )
	{
		// not error, not warning, just notify
		MsgDev( D_NOTE, "UI_SetColors: scripts/colors.lst not found\n" );
		return;
	}

	while( script )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			break;

		if( !com.stricmp( token.string, "HELP_COLOR" ))
		{
			UI_ParseColor( script, uiColorHelp );
		}
		else if( !com.stricmp( token.string, "PROMPT_BG_COLOR" ))
		{
			UI_ParseColor( script, uiPromptBgColor );
		}
		else if( !com.stricmp( token.string, "PROMPT_TEXT_COLOR" ))
		{
			UI_ParseColor( script, uiPromptTextColor );
		}
		else if( !com.stricmp( token.string, "PROMPT_FOCUS_COLOR" ))
		{
			UI_ParseColor( script, uiPromptFocusColor );
		}
		else if( !com.stricmp( token.string, "INPUT_TEXT_COLOR" ))
		{
			UI_ParseColor( script, uiInputTextColor );
		}
		else if( !com.stricmp( token.string, "INPUT_BG_COLOR" ))
		{
			UI_ParseColor( script, uiInputBgColor );
		}
		else if( !com.stricmp( token.string, "INPUT_FG_COLOR" ))
		{
			UI_ParseColor( script, uiInputFgColor );
		}
	}
	Com_CloseScript( script );
}

/*
=================
UI_Init
=================
*/
void UI_Init( void )
{
	// register our cvars and commands
	ui_precache = Cvar_Get( "ui_precache", "0", CVAR_ARCHIVE, "enable precache all resources for menu" );
	ui_sensitivity = Cvar_Get( "ui_sensitivity", "1", CVAR_ARCHIVE, "mouse sensitivity while in-menu" );
	ui_mainfont = Cvar_Get( "ui_mainfont", "default", CVAR_ARCHIVE, "ui primary font (buttons, title, etc)" );
	ui_namefont = Cvar_Get( "ui_namefont", "default", CVAR_ARCHIVE, "ui console font (hints, notify)" );

	Cmd_AddCommand( "menufont", UI_SetFont_f, "set menu master/notify font" );
	Cmd_AddCommand( "menu_main", UI_Main_Menu, "open the main menu" );
	Cmd_AddCommand( "menu_newgame", UI_NewGame_Menu, "open the newgame menu" );
	Cmd_AddCommand( "menu_loadgame", UI_LoadGame_Menu, "open the loadgame menu" );
	Cmd_AddCommand( "menu_savegame", UI_SaveGame_Menu, "open the savegame menu" );
	Cmd_AddCommand( "menu_saveload", UI_SaveLoad_Menu, "open the save\\load menu" );
	Cmd_AddCommand( "menu_recdemo", UI_RecDemo_Menu, "open the record demo menu" );
	Cmd_AddCommand( "menu_playdemo", UI_PlayDemo_Menu, "open the playback demo menu" );
	Cmd_AddCommand( "menu_playrec", UI_PlayRec_Menu, "open the play\\record demo menu" );
	Cmd_AddCommand( "menu_multiplayer", UI_MultiPlayer_Menu, "open the multiplayer menu" );
	Cmd_AddCommand( "menu_options", UI_Options_Menu, "open the options menu" );
	Cmd_AddCommand( "menu_langame", UI_LanGame_Menu, "open the LAN game menu" );
	Cmd_AddCommand( "menu_playersetup", UI_PlayerSetup_Menu, "open the player setup menu" );
	Cmd_AddCommand( "menu_controls", UI_Controls_Menu, "open the controls menu" );
	Cmd_AddCommand( "menu_advcontrols", UI_AdvControls_Menu, "open the advanced controls menu" );
	Cmd_AddCommand( "menu_gameoptions", UI_GameOptions_Menu, "open the game options menu" );
	Cmd_AddCommand( "menu_creategame", UI_CreateGame_Menu, "open the create LAN game menu" );
	Cmd_AddCommand( "menu_audio", UI_Audio_Menu, "open the sound options menu" );
	Cmd_AddCommand( "menu_video", UI_Video_Menu, "open the video settings head menu" );
	Cmd_AddCommand( "menu_vidoptions", UI_VidOptions_Menu, "open the video options menu" );
	Cmd_AddCommand( "menu_vidmodes", UI_VidModes_Menu, "open the video modes menu" );
	Cmd_AddCommand( "menu_customgame", UI_CustomGame_Menu, "open the change game menu" );

	Mem_Set( uiEmptyString, ' ', sizeof( uiEmptyString ));
	uiStatic.scaleX = scr_width->integer / 1024.0f;
	uiStatic.scaleY = scr_height->integer / 768.0f;

	// move cursor to screen center
	uiStatic.cursorX = scr_width->integer>>1;
	uiStatic.cursorY = scr_height->integer>>1;
	uiStatic.outlineWidth = 4;
	uiStatic.sliderWidth = 6;

	UI_ScaleCoords( NULL, NULL, &uiStatic.outlineWidth, NULL );
	UI_ScaleCoords( NULL, NULL, &uiStatic.sliderWidth, NULL );

	// trying to load colors.lst
	UI_ApplyCustomColors ();

	if( re )
	{
		// register ui fonts
		uiStatic.nameFont = re->RegisterShader( va( "gfx/fonts/%s", ui_namefont->string ), SHADER_FONT );
		uiStatic.menuFont = re->RegisterShader( va( "gfx/fonts/%s", ui_mainfont->string ), SHADER_FONT );
	}
	uiStatic.initialized = true;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown( void )
{
	if( !uiStatic.initialized )
		return;

	Cmd_RemoveCommand( "menufont" );
	Cmd_RemoveCommand( "menu_main" );
	Cmd_RemoveCommand( "menu_newgame" );
	Cmd_RemoveCommand( "menu_loadgame" );
	Cmd_RemoveCommand( "menu_savegame" );
	Cmd_RemoveCommand( "menu_saveload" );
	Cmd_RemoveCommand( "menu_recdemo" );
	Cmd_RemoveCommand( "menu_playdemo" );
	Cmd_RemoveCommand( "menu_playrec" );
	Cmd_RemoveCommand( "menu_multiplayer" );
	Cmd_RemoveCommand( "menu_options" );
	Cmd_RemoveCommand( "menu_langame" );
	Cmd_RemoveCommand( "menu_playersetup" );
	Cmd_RemoveCommand( "menu_controls" );
	Cmd_RemoveCommand( "menu_advcontrols" );
	Cmd_RemoveCommand( "menu_gameoptions" );
	Cmd_RemoveCommand( "menu_creategame" );
	Cmd_RemoveCommand( "menu_audio" );
	Cmd_RemoveCommand( "menu_video" );
	Cmd_RemoveCommand( "menu_vidoptions" );
	Cmd_RemoveCommand( "menu_vidmodes" );
	Cmd_RemoveCommand( "menu_advanced" );
	Cmd_RemoveCommand( "menu_performance" );
	Cmd_RemoveCommand( "menu_network" );
	Cmd_RemoveCommand( "menu_defaults" );
	Cmd_RemoveCommand( "menu_cinematics" );
	Cmd_RemoveCommand( "menu_customgame" );
	Cmd_RemoveCommand( "menu_quit" );

	Mem_Set( &uiStatic, 0, sizeof( uiStatic_t ));
}
