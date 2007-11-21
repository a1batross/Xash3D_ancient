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
#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "client.h"
#include "qmenu.h"

static int	m_main_cursor;

#define NUM_CURSOR_FRAMES	15
#define CHAR_STEP		20

static char *menu_in_sound	= "misc/menu1.wav";
static char *menu_move_sound	= "misc/menu2.wav";
static char *menu_out_sound	= "misc/menu3.wav";

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

bool	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

static void M_Banner( char *name )
{
	int w, h;

	re->DrawGetPicSize (&w, &h, name );
	SCR_DrawPic( SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 2 - 110, w, h, name );
}

void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (Cvar_VariableValue ("maxclients") == 1 && Host_ServerState ())
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i = 0; i < m_menudepth; i++)
	{
		if (m_layers[i].draw == draw && m_layers[i].key == key)
		{
			m_menudepth = i;
		}
	}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
		{
			MsgWarn("M_PushMenu: depth == MAX_MENU_DEPTH\n");
			return;
		}
		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;
	m_entersound = true;
	cls.key_dest = key_menu;
}

void M_ForceMenuOff (void)
{
	m_drawfunc = 0;
	m_keyfunc = 0;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates();
	Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	S_StartLocalSound( menu_out_sound );
	if (m_menudepth < 1)
	{
		MsgWarn("M_PopMenu: depth == 0");
		return;
	}
	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth) M_ForceMenuOff ();
}


const char *Default_MenuKey( menuframework_s *m, int key )
{
	const char *sound = NULL;
	menucommon_s *item;

	if ( m )
	{
		if (( item = Menu_ItemAtCursor( m )) != 0 )
		{
			if( item->type == MTYPE_FIELD )
			{
				if( Field_Key((menufield_s * )item, key ))
					return NULL;
			}
		}
	}

	switch( key )
	{
	case K_ESCAPE:
		M_PopMenu();
		return menu_out_sound;
	case K_KP_UPARROW:
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_MOUSE4:
	case K_MOUSE5:
	case K_KP_ENTER:
	case K_ENTER:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
	SCR_DrawSmallChar( cx + ((SCREEN_WIDTH - SMALLCHAR_WIDTH)>>1), cy + ((SCREEN_HEIGHT - SMALLCHAR_HEIGHT)>>1), num);
}

void M_Print (int cx, int cy, char *str)
{
	SCR_DrawSmallStringExt( cx, cy, str, g_color_table[3], true );
}

void M_PrintWhite (int cx, int cy, char *str)
{
	SCR_DrawSmallStringExt( cx, cy, str, g_color_table[7], true );
}

void M_DrawPic (int x, int y, char *pic)
{
	SCR_DrawPic(x + ((SCREEN_WIDTH - 320)>>1), y + ((SCREEN_HEIGHT - 240)>>1), -1, -1, pic);
}


/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y.  The pic will extend to the left of x,
and both above and below y.
=============
*/
void M_DrawCursor( int x, int y, int f )
{
	char	cursorname[80];
	static bool cached;

	if ( !cached )
	{
		int i;

		for ( i = 0; i < NUM_CURSOR_FRAMES; i++ )
		{
			sprintf( cursorname, "menu/m_cursor%d", i );
			re->RegisterPic( cursorname );
		}
		cached = true;
	}

	sprintf( cursorname, "menu/m_cursor%d", f );
	SCR_DrawPic( x, y, -1, -1, cursorname );
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter(cx, cy, 4);
	}
	M_DrawCharacter (cx, cy+8, 7);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx, cy, 5);
		}
		M_DrawCharacter (cx, cy+8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 6);
	}
	M_DrawCharacter (cx, cy+8, 9);
}

		
/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5


void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	char *names[] =
	{
		"menu/m_main_game",
		"menu/m_main_multiplayer",
		"menu/m_main_options",
		"menu/m_main_video",
		"menu/m_main_quit",
		0
	};

	for ( i = 0; names[i] != 0; i++ )
	{
		re->DrawGetPicSize( &w, &h, names[i] );

		if ( w > widest ) widest = w;
		totalheight += ( h + 12 );
	}

	ystart = ( SCREEN_HEIGHT / 2 - 110 );
	xoffset = ( SCREEN_WIDTH - widest + 70 ) / 2;

	for ( i = 0; names[i] != 0; i++ )
	{
		if ( i != m_main_cursor )
			SCR_DrawPic( xoffset, ystart + i * 40 + 13, -1, -1, names[i] );
	}
	strcpy( litname, names[m_main_cursor] );
	strncat( litname, "_sel", 80 );
	SCR_DrawPic( xoffset, ystart + m_main_cursor * 40 + 13, -1, -1, litname );

	M_DrawCursor( xoffset - 25, ystart + m_main_cursor * 40 + 11, (int)(cls.realtime * 8.0f) % NUM_CURSOR_FRAMES );

	re->DrawGetPicSize( &w, &h, "menu/m_main_plaque" );
	SCR_DrawPic( xoffset - 30 - w, ystart, w, h, "menu/m_main_plaque" );
	SCR_DrawPic( xoffset - 30 - w, ystart + h + 5, -1, -1, "menu/m_main_logo" );
}


const char *M_Main_Key(int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_ESCAPE:
		M_PopMenu();
		break;

	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_Game_f ();
			break;

		case 1:
			M_Menu_Multiplayer_f();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Video_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}

	return NULL;
}


void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/
static menuframework_s	s_multiplayer_menu;
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;

static void Multiplayer_MenuDraw (void)
{
	M_Banner( "menu/m_banner_multiplayer" );

	Menu_AdjustCursor( &s_multiplayer_menu, 1 );
	Menu_Draw( &s_multiplayer_menu );
}

static void PlayerSetupFunc( void *unused )
{
	M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc( void *unused )
{
	M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc( void *unused )
{
	M_Menu_StartServer_f ();
}

void Multiplayer_MenuInit( void )
{
	float	x = SCREEN_WIDTH * 0.50 - 64;

	SCR_AdjustSize( &x, NULL, NULL, NULL );

	s_multiplayer_menu.x = x;
	s_multiplayer_menu.nitems = 0;

	s_join_network_server_action.generic.type	= MTYPE_ACTION;
	s_join_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x		= 0;
	s_join_network_server_action.generic.y		= 0;
	s_join_network_server_action.generic.name	= " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	= MTYPE_ACTION;
	s_start_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x		= 0;
	s_start_network_server_action.generic.y		= 17;
	s_start_network_server_action.generic.name	= " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type	= MTYPE_ACTION;
	s_player_setup_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x		= 0;
	s_player_setup_action.generic.y		= 34;
	s_player_setup_action.generic.name	= " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_join_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_start_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_player_setup_action );

	Menu_SetStatusBar( &s_multiplayer_menu, NULL );

	Menu_Center( &s_multiplayer_menu );
}

const char *Multiplayer_MenuKey( int key )
{
	return Default_MenuKey( &s_multiplayer_menu, key );
}

void M_Menu_Multiplayer_f( void )
{
	Multiplayer_MenuInit();
	M_PushMenu( Multiplayer_MenuDraw, Multiplayer_MenuKey );
}

/*
=======================================================================

KEYS MENU

=======================================================================
*/
char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"weapnext", 		"next weapon"},
{"+forward", 		"walk forward"},
{"+back", 		"backpedal"},
{"+left", 		"turn left"},
{"+right", 		"turn right"},
{"+speed", 		"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 		"mouse look"},
{"+klook", 		"keyboard look"},
{"+moveup",		"up / jump"},
{"+movedown",		"down / crouch"},

{"inven",			"inventory"},
{"invuse",		"use item"},
{"invdrop",		"drop item"},
{"invprev",		"prev item"},
{"invnext",		"next item"},

{"cmd help", 		"help computer" }, 
{ 0, 0 }
};

int			keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s		s_keys_attack_action;
static menuaction_s		s_keys_change_weapon_action;
static menuaction_s		s_keys_walk_forward_action;
static menuaction_s		s_keys_backpedal_action;
static menuaction_s		s_keys_turn_left_action;
static menuaction_s		s_keys_turn_right_action;
static menuaction_s		s_keys_run_action;
static menuaction_s		s_keys_step_left_action;
static menuaction_s		s_keys_step_right_action;
static menuaction_s		s_keys_sidestep_action;
static menuaction_s		s_keys_look_up_action;
static menuaction_s		s_keys_look_down_action;
static menuaction_s		s_keys_center_view_action;
static menuaction_s		s_keys_mouse_look_action;
static menuaction_s		s_keys_keyboard_look_action;
static menuaction_s		s_keys_move_up_action;
static menuaction_s		s_keys_move_down_action;
static menuaction_s		s_keys_inventory_action;
static menuaction_s		s_keys_inv_use_action;
static menuaction_s		s_keys_inv_drop_action;
static menuaction_s		s_keys_inv_prev_action;
static menuaction_s		s_keys_inv_next_action;
static menuaction_s		s_keys_help_computer_action;

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char		*b;

	l = strlen(command);

	for (j = 0; j < 256; j++)
	{
		b = Key_IsBind(j);
		if (!b) continue;
		if (!strncmp(b, command, l) )
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand(char *command, int *twokeys)
{
	int	count;
	int	j;
	int	l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		b = Key_IsBind(j);
		if (!b) continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2) break;
		}
	}
}

static void KeyCursorDrawFunc( menuframework_s *menu )
{
	if ( bind_grab ) SCR_DrawSmallChar( menu->x, menu->y + menu->cursor * 9, '=' );
	else SCR_DrawSmallChar( menu->x, menu->y + menu->cursor * 9, 12 + (( int )(cls.realtime * 5.0f) & 1 ));
}

static void DrawKeyBindingFunc( void *self )
{
	int keys[2];
	menuaction_s *a = ( menuaction_s * ) self;

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys);
		
	if (keys[0] == -1)
	{
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???" );
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (keys[0]);
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name );

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString( a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, "or" );
			Menu_DrawString( a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, Key_KeynumToString (keys[1]) );
		}
	}
}

static void KeyBindingFunc( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;
	int keys[2];

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys );
	if(keys[1] != -1) M_UnbindCommand( bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;
	Menu_SetStatusBar( &s_keys_menu, "press a key or button for this action" );
}

static void Keys_MenuInit( void )
{
	int	i = 0;
	float	x = SCREEN_WIDTH * 0.50;
	float	y = 0;

	SCR_AdjustSize( &x, &y, NULL, NULL );
	s_keys_menu.x = x;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	s_keys_attack_action.generic.type = MTYPE_ACTION;
	s_keys_attack_action.generic.flags = QMF_GRAYED;
	s_keys_attack_action.generic.x = 0;
	s_keys_attack_action.generic.y = y;
	s_keys_attack_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_attack_action.generic.localdata[0] = i;
	s_keys_attack_action.generic.name = bindnames[s_keys_attack_action.generic.localdata[0]][1];

	s_keys_change_weapon_action.generic.type = MTYPE_ACTION;
	s_keys_change_weapon_action.generic.flags = QMF_GRAYED;
	s_keys_change_weapon_action.generic.x = 0;
	s_keys_change_weapon_action.generic.y = y += 17;
	s_keys_change_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_change_weapon_action.generic.localdata[0] = ++i;
	s_keys_change_weapon_action.generic.name	= bindnames[s_keys_change_weapon_action.generic.localdata[0]][1];

	s_keys_walk_forward_action.generic.type	= MTYPE_ACTION;
	s_keys_walk_forward_action.generic.flags = QMF_GRAYED;
	s_keys_walk_forward_action.generic.x = 0;
	s_keys_walk_forward_action.generic.y = y += 17;
	s_keys_walk_forward_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_walk_forward_action.generic.localdata[0] = ++i;
	s_keys_walk_forward_action.generic.name	= bindnames[s_keys_walk_forward_action.generic.localdata[0]][1];

	s_keys_backpedal_action.generic.type = MTYPE_ACTION;
	s_keys_backpedal_action.generic.flags = QMF_GRAYED;
	s_keys_backpedal_action.generic.x = 0;
	s_keys_backpedal_action.generic.y = y += 17;
	s_keys_backpedal_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_backpedal_action.generic.localdata[0] = ++i;
	s_keys_backpedal_action.generic.name = bindnames[s_keys_backpedal_action.generic.localdata[0]][1];

	s_keys_turn_left_action.generic.type = MTYPE_ACTION;
	s_keys_turn_left_action.generic.flags = QMF_GRAYED;
	s_keys_turn_left_action.generic.x = 0;
	s_keys_turn_left_action.generic.y = y += 17;
	s_keys_turn_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_left_action.generic.localdata[0] = ++i;
	s_keys_turn_left_action.generic.name = bindnames[s_keys_turn_left_action.generic.localdata[0]][1];

	s_keys_turn_right_action.generic.type = MTYPE_ACTION;
	s_keys_turn_right_action.generic.flags = QMF_GRAYED;
	s_keys_turn_right_action.generic.x = 0;
	s_keys_turn_right_action.generic.y = y += 17;
	s_keys_turn_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_right_action.generic.localdata[0] = ++i;
	s_keys_turn_right_action.generic.name = bindnames[s_keys_turn_right_action.generic.localdata[0]][1];

	s_keys_run_action.generic.type = MTYPE_ACTION;
	s_keys_run_action.generic.flags = QMF_GRAYED;
	s_keys_run_action.generic.x = 0;
	s_keys_run_action.generic.y = y += 17;
	s_keys_run_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_run_action.generic.localdata[0] = ++i;
	s_keys_run_action.generic.name = bindnames[s_keys_run_action.generic.localdata[0]][1];

	s_keys_step_left_action.generic.type = MTYPE_ACTION;
	s_keys_step_left_action.generic.flags = QMF_GRAYED;
	s_keys_step_left_action.generic.x = 0;
	s_keys_step_left_action.generic.y = y += 17;
	s_keys_step_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_left_action.generic.localdata[0] = ++i;
	s_keys_step_left_action.generic.name = bindnames[s_keys_step_left_action.generic.localdata[0]][1];

	s_keys_step_right_action.generic.type = MTYPE_ACTION;
	s_keys_step_right_action.generic.flags = QMF_GRAYED;
	s_keys_step_right_action.generic.x = 0;
	s_keys_step_right_action.generic.y = y += 17;
	s_keys_step_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_right_action.generic.localdata[0] = ++i;
	s_keys_step_right_action.generic.name = bindnames[s_keys_step_right_action.generic.localdata[0]][1];

	s_keys_sidestep_action.generic.type = MTYPE_ACTION;
	s_keys_sidestep_action.generic.flags = QMF_GRAYED;
	s_keys_sidestep_action.generic.x = 0;
	s_keys_sidestep_action.generic.y = y += 17;
	s_keys_sidestep_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_sidestep_action.generic.localdata[0] = ++i;
	s_keys_sidestep_action.generic.name = bindnames[s_keys_sidestep_action.generic.localdata[0]][1];

	s_keys_look_up_action.generic.type = MTYPE_ACTION;
	s_keys_look_up_action.generic.flags = QMF_GRAYED;
	s_keys_look_up_action.generic.x = 0;
	s_keys_look_up_action.generic.y = y += 17;
	s_keys_look_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_up_action.generic.localdata[0] = ++i;
	s_keys_look_up_action.generic.name = bindnames[s_keys_look_up_action.generic.localdata[0]][1];

	s_keys_look_down_action.generic.type = MTYPE_ACTION;
	s_keys_look_down_action.generic.flags = QMF_GRAYED;
	s_keys_look_down_action.generic.x = 0;
	s_keys_look_down_action.generic.y = y += 17;
	s_keys_look_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_down_action.generic.localdata[0] = ++i;
	s_keys_look_down_action.generic.name = bindnames[s_keys_look_down_action.generic.localdata[0]][1];

	s_keys_center_view_action.generic.type = MTYPE_ACTION;
	s_keys_center_view_action.generic.flags = QMF_GRAYED;
	s_keys_center_view_action.generic.x = 0;
	s_keys_center_view_action.generic.y = y += 17;
	s_keys_center_view_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_center_view_action.generic.localdata[0] = ++i;
	s_keys_center_view_action.generic.name = bindnames[s_keys_center_view_action.generic.localdata[0]][1];

	s_keys_mouse_look_action.generic.type = MTYPE_ACTION;
	s_keys_mouse_look_action.generic.flags = QMF_GRAYED;
	s_keys_mouse_look_action.generic.x = 0;
	s_keys_mouse_look_action.generic.y = y += 17;
	s_keys_mouse_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_mouse_look_action.generic.localdata[0] = ++i;
	s_keys_mouse_look_action.generic.name = bindnames[s_keys_mouse_look_action.generic.localdata[0]][1];

	s_keys_keyboard_look_action.generic.type = MTYPE_ACTION;
	s_keys_keyboard_look_action.generic.flags = QMF_GRAYED;
	s_keys_keyboard_look_action.generic.x = 0;
	s_keys_keyboard_look_action.generic.y = y += 17;
	s_keys_keyboard_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_keyboard_look_action.generic.localdata[0] = ++i;
	s_keys_keyboard_look_action.generic.name = bindnames[s_keys_keyboard_look_action.generic.localdata[0]][1];

	s_keys_move_up_action.generic.type = MTYPE_ACTION;
	s_keys_move_up_action.generic.flags = QMF_GRAYED;
	s_keys_move_up_action.generic.x = 0;
	s_keys_move_up_action.generic.y = y += 17;
	s_keys_move_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_up_action.generic.localdata[0] = ++i;
	s_keys_move_up_action.generic.name = bindnames[s_keys_move_up_action.generic.localdata[0]][1];

	s_keys_move_down_action.generic.type = MTYPE_ACTION;
	s_keys_move_down_action.generic.flags = QMF_GRAYED;
	s_keys_move_down_action.generic.x = 0;
	s_keys_move_down_action.generic.y = y += 17;
	s_keys_move_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_down_action.generic.localdata[0] = ++i;
	s_keys_move_down_action.generic.name = bindnames[s_keys_move_down_action.generic.localdata[0]][1];

	s_keys_inventory_action.generic.type = MTYPE_ACTION;
	s_keys_inventory_action.generic.flags = QMF_GRAYED;
	s_keys_inventory_action.generic.x = 0;
	s_keys_inventory_action.generic.y = y += 17;
	s_keys_inventory_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inventory_action.generic.localdata[0] = ++i;
	s_keys_inventory_action.generic.name = bindnames[s_keys_inventory_action.generic.localdata[0]][1];

	s_keys_inv_use_action.generic.type = MTYPE_ACTION;
	s_keys_inv_use_action.generic.flags = QMF_GRAYED;
	s_keys_inv_use_action.generic.x = 0;
	s_keys_inv_use_action.generic.y = y += 17;
	s_keys_inv_use_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_use_action.generic.localdata[0] = ++i;
	s_keys_inv_use_action.generic.name = bindnames[s_keys_inv_use_action.generic.localdata[0]][1];

	s_keys_inv_drop_action.generic.type = MTYPE_ACTION;
	s_keys_inv_drop_action.generic.flags = QMF_GRAYED;
	s_keys_inv_drop_action.generic.x = 0;
	s_keys_inv_drop_action.generic.y = y += 17;
	s_keys_inv_drop_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_drop_action.generic.localdata[0] = ++i;
	s_keys_inv_drop_action.generic.name = bindnames[s_keys_inv_drop_action.generic.localdata[0]][1];

	s_keys_inv_prev_action.generic.type = MTYPE_ACTION;
	s_keys_inv_prev_action.generic.flags = QMF_GRAYED;
	s_keys_inv_prev_action.generic.x = 0;
	s_keys_inv_prev_action.generic.y = y += 17;
	s_keys_inv_prev_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_prev_action.generic.localdata[0] = ++i;
	s_keys_inv_prev_action.generic.name = bindnames[s_keys_inv_prev_action.generic.localdata[0]][1];

	s_keys_inv_next_action.generic.type = MTYPE_ACTION;
	s_keys_inv_next_action.generic.flags = QMF_GRAYED;
	s_keys_inv_next_action.generic.x = 0;
	s_keys_inv_next_action.generic.y = y += 17;
	s_keys_inv_next_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_next_action.generic.localdata[0] = ++i;
	s_keys_inv_next_action.generic.name = bindnames[s_keys_inv_next_action.generic.localdata[0]][1];

	s_keys_help_computer_action.generic.type = MTYPE_ACTION;
	s_keys_help_computer_action.generic.flags = QMF_GRAYED;
	s_keys_help_computer_action.generic.x = 0;
	s_keys_help_computer_action.generic.y = y += 17;
	s_keys_help_computer_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_help_computer_action.generic.localdata[0] = ++i;
	s_keys_help_computer_action.generic.name = bindnames[s_keys_help_computer_action.generic.localdata[0]][1];

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_attack_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_change_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_walk_forward_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_backpedal_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_run_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_sidestep_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_center_view_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_mouse_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_keyboard_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inventory_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_use_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_drop_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_prev_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_next_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_help_computer_action );
	
	Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
	Menu_Center( &s_keys_menu );
}

static void Keys_MenuDraw (void)
{
	Menu_AdjustCursor( &s_keys_menu, 1 );
	Menu_Draw( &s_keys_menu );
}

static const char *Keys_MenuKey( int key )
{
	menuaction_s *item = ( menuaction_s * ) Menu_ItemAtCursor( &s_keys_menu );

	if ( bind_grab )
	{	
		if(key != K_ESCAPE && key != '`')
		{
			char cmd[1024];

			sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}
		
		Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
		bind_grab = false;
		return menu_out_sound;
	}

	switch ( key )
	{
	case K_KP_ENTER:
	case K_ENTER:
		KeyBindingFunc( item );
		return menu_in_sound;
	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
		M_UnbindCommand( bindnames[item->generic.localdata[0]][0] );
		return menu_out_sound;
	default:
		return Default_MenuKey( &s_keys_menu, key );
	}
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit();
	M_PushMenu( Keys_MenuDraw, Keys_MenuKey );
}


/*
=======================================================================

CONTROLS MENU

=======================================================================
*/
static menuframework_s	s_options_menu;
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
static menuslider_s		s_options_musicvolume_slider;
static menulist_s		s_options_quality_list;
static menulist_s		s_options_console_action;

static void CrosshairFunc( void *unused )
{
	Cvar_SetValue( "crosshair", s_options_crosshair_box.curvalue );
}

static void CustomizeControlsFunc( void *unused )
{
	M_Menu_Keys_f();
}

static void AlwaysRunFunc( void *unused )
{
	Cvar_SetValue( "cl_run", s_options_alwaysrun_box.curvalue );
}

static void FreeLookFunc( void *unused )
{
	Cvar_SetValue( "freelook", s_options_freelook_box.curvalue );
}

static void MouseSpeedFunc( void *unused )
{
	Cvar_SetValue( "sensitivity", s_options_sensitivity_slider.curvalue / 2.0F );
}

static float ClampCvar( float min, float max, float value )
{
	return bound(min, value, max);
}

static void ControlsSetMenuItemValues( void )
{
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue( "s_volume" ) * 10;
	s_options_musicvolume_slider.curvalue = Cvar_VariableValue( "s_musicvolume" ) * 10;
	s_options_quality_list.curvalue = !(Cvar_VariableValue( "s_khz" ) == 22);
	s_options_sensitivity_slider.curvalue = ( sensitivity->value ) * 2;

	Cvar_SetValue( "cl_run", ClampCvar( 0, 1, cl_run->value ) );
	s_options_alwaysrun_box.curvalue = cl_run->value;

	s_options_invertmouse_box.curvalue = m_pitch->value < 0;

	Cvar_SetValue( "lookspring", ClampCvar( 0, 1, lookspring->value ));
	s_options_lookspring_box.curvalue = lookspring->value;

	Cvar_SetValue( "lookstrafe", ClampCvar( 0, 1, lookstrafe->value ));
	s_options_lookstrafe_box.curvalue = lookstrafe->value;

	Cvar_SetValue( "freelook", ClampCvar( 0, 1, freelook->value ));
	s_options_freelook_box.curvalue = freelook->value;

	Cvar_SetValue( "crosshair", ClampCvar( 0, 3, crosshair->value ));
	s_options_crosshair_box.curvalue = crosshair->value;
}

static void ControlsResetDefaultsFunc( void *unused )
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute();

	ControlsSetMenuItemValues();
}

static void InvertMouseFunc( void *unused )
{
	Cvar_SetValue( "m_pitch", -m_pitch->value );
}

static void LookspringFunc( void *unused )
{
	Cvar_SetValue( "lookspring", !lookspring->value );
}

static void LookstrafeFunc( void *unused )
{
	Cvar_SetValue( "lookstrafe", !lookstrafe->value );
}

static void UpdateVolumeFunc( void *unused )
{
	Cvar_SetValue( "s_volume", s_options_sfxvolume_slider.curvalue / 10 );
}

static void UpdateMusicVolumeFunc( void *unused )
{
	Cvar_SetValue( "s_musicvolume", s_options_musicvolume_slider.curvalue / 10 );
}

static void ConsoleFunc( void *unused )
{
	/*
	** the proper way to do this is probably to have ToggleConsole_f accept a parameter
	*/

	Field_Clear( &g_consoleField );
	Con_ClearNotify ();

	M_ForceMenuOff();
	cls.key_dest = key_console;
}

static void UpdateSoundQualityFunc( void *unused )
{
	if ( s_options_quality_list.curvalue ) Cvar_SetValue( "s_khz", 44 );
	else Cvar_SetValue( "s_khz", 22 );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	re->EndFrame();
	CL_Snd_Restart_f();
}

void Options_MenuInit( void )
{
	static const char *quality_items[] =
	{
		"low", 
		"high",
		0
	};

	static const char *compatibility_items[] =
	{
		"max compatibility",
		"max performance",
		0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};
	float x = SCREEN_WIDTH / 2;
	float y = SCREEN_HEIGHT / 2 - 58;

	/*
	** configure controls menu and menu items
	*/
	SCR_AdjustSize( &x, &y, NULL, NULL );
	s_options_menu.x = x;
	s_options_menu.y = y;
	s_options_menu.nitems = 0;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x = 0;
	s_options_sfxvolume_slider.generic.y = 0;
	s_options_sfxvolume_slider.generic.name	= "sound volume";
	s_options_sfxvolume_slider.generic.callback = UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue = 0;
	s_options_sfxvolume_slider.maxvalue = 10;
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue( "s_volume" ) * 10;

	s_options_musicvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_musicvolume_slider.generic.x = 0;
	s_options_musicvolume_slider.generic.y = 17;
	s_options_musicvolume_slider.generic.name = "music volume";
	s_options_musicvolume_slider.generic.callback = UpdateMusicVolumeFunc;
	s_options_musicvolume_slider.minvalue = 0;
	s_options_musicvolume_slider.maxvalue = 10;
	s_options_musicvolume_slider.curvalue = Cvar_VariableValue( "s_musicvolume" ) * 10;

	s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x = 0;
	s_options_quality_list.generic.y = 34;
	s_options_quality_list.generic.name = "sound quality";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames = quality_items;
	s_options_quality_list.curvalue = !(Cvar_VariableValue( "s_khz" ) == 22);

	s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x = 0;
	s_options_sensitivity_slider.generic.y = 68;
	s_options_sensitivity_slider.generic.name = "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue = 2;
	s_options_sensitivity_slider.maxvalue = 22;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x = 0;
	s_options_alwaysrun_box.generic.y = 85;
	s_options_alwaysrun_box.generic.name = "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x = 0;
	s_options_invertmouse_box.generic.y = 102;
	s_options_invertmouse_box.generic.name	= "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookspring_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookspring_box.generic.x	= 0;
	s_options_lookspring_box.generic.y	= 119;
	s_options_lookspring_box.generic.name	= "lookspring";
	s_options_lookspring_box.generic.callback = LookspringFunc;
	s_options_lookspring_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x	= 0;
	s_options_lookstrafe_box.generic.y	= 136;
	s_options_lookstrafe_box.generic.name	= "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x	= 0;
	s_options_freelook_box.generic.y	= 153;
	s_options_freelook_box.generic.name	= "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x	= 0;
	s_options_crosshair_box.generic.y	= 170;
	s_options_crosshair_box.generic.name	= "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x = 0;
	s_options_customize_options_action.generic.y = 221;
	s_options_customize_options_action.generic.name = "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x = 0;
	s_options_defaults_action.generic.y = 238;
	s_options_defaults_action.generic.name	= "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type = MTYPE_ACTION;
	s_options_console_action.generic.x = 0;
	s_options_console_action.generic.y = 255;
	s_options_console_action.generic.name = "go to console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues();

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sfxvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_musicvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_quality_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sensitivity_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_alwaysrun_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_invertmouse_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookspring_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookstrafe_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_freelook_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_crosshair_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );

	if(!host.debug && !host.developer) return;
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}

void Options_MenuDraw (void)
{
	M_Banner( "menu/m_banner_options" );
	Menu_AdjustCursor( &s_options_menu, 1 );
	Menu_Draw( &s_options_menu );
}

const char *Options_MenuKey( int key )
{
	return Default_MenuKey( &s_options_menu, key );
}

void M_Menu_Options_f (void)
{
	Options_MenuInit();
	M_PushMenu ( Options_MenuDraw, Options_MenuKey );
}

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

void M_Menu_Video_f (void)
{
	VID_MenuInit();
	M_PushMenu( VID_MenuDraw, VID_MenuKey );
}

/*
=============================================================================

END GAME MENU

=============================================================================
*/
static float credits_start_time;
static float credits_fade_time;
static float credits_show_time;
static const char **credits;
static char *creditsIndex[1024];
static char *creditsBuffer;
static uint credit_numlines;

static const char *xash_credits[] =
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
	"^3THANKS TO"
	"ID Software at all",
	"Lord Havoc (Darkplaces Team)",
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
	"Copyright XashXT Group 2007 (C)",
	0
};

void M_Credits_MenuDraw( void )
{
	int	i, x, y;
	float	*color;

	y = SCREEN_HEIGHT - (( cls.realtime - credits_start_time ) * 40.0f );

	// draw the credits
	for ( i = 0; i < credit_numlines && credits[i]; i++, y += 20 )
	{
		// skip not visible lines, but always draw end line
		if( y <= -16 && i != credit_numlines - 1) continue;
		x = ( SCREEN_WIDTH - BIGCHAR_WIDTH * std.cstrlen( credits[i] ))/2;

		if((y < (SCREEN_HEIGHT - BIGCHAR_HEIGHT) / 2) && i == credit_numlines - 1)
		{
			Msg("draw last\n");
			if(!credits_fade_time) credits_fade_time = cls.realtime;
			color = CG_FadeColor( credits_fade_time, credits_show_time );
			if( color )SCR_DrawBigStringColor( x, (SCREEN_HEIGHT-BIGCHAR_HEIGHT)/2, credits[i], color);
		}
		else SCR_DrawBigString( x, y, credits[i], 1.0f );
	}

	if( y < 0 && !color ) M_PopMenu(); // end of credits
}

const char *M_Credits_Key( int key )
{
	switch( key )
	{
	case K_ESCAPE:
		M_PopMenu();
		break;
	}
	return NULL;
}

void M_Menu_Credits_f( void )
{
	int		count;
	char		*p;

	if(!creditsBuffer)
	{
		// load credits if needed
		creditsBuffer = FS_LoadFile( "scripts/credits.txt", &count );
		if(count)
		{
			if(creditsBuffer[count - 1] != '\n' && creditsBuffer[count - 1] != '\r')
			{
				creditsBuffer = Mem_Realloc( zonepool, creditsBuffer, count + 2 );
				strncpy( creditsBuffer + count, "\r", 1 ); // add terminator
				count += 2;
                    	}

			p = creditsBuffer;
			for (credit_numlines = 0; credit_numlines < 1024; credit_numlines++)
			{
				creditsIndex[credit_numlines] = p;
				while (*p != '\r' && *p != '\n')
				{
					p++;
					if (--count == 0) break;
				}
				if (*p == '\r')
				{
					*p++ = 0;
					if (--count == 0) break;
				}
				*p++ = 0;
				if (--count == 0) break;
			}
			creditsIndex[++credit_numlines] = 0;
			credits = creditsIndex;
		}
		else
		{
			credits = xash_credits;
			credit_numlines = (sizeof(xash_credits) / sizeof(xash_credits[0])) - 1; // skip term
		}
	}	

	// run credits
	credits_start_time = cls.realtime;
	credits_show_time = bound(0.1f, (float)std.strlen(credits[credit_numlines - 1]), 12.0f );
	credits_fade_time = 0.0f;
	M_PushMenu( M_Credits_MenuDraw, M_Credits_Key);
}

/*
=============================================================================

GAME MENU

=============================================================================
*/

static int		m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;

static void StartGame( void )
{
	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff();
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );
	Cvar_SetValue( "gamerules", 0 );

	Cbuf_AddText("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

static void EasyGameFunc( void *data )
{
	Cvar_SetLatched( "skill", "0" );
	StartGame();
}

static void MediumGameFunc( void *data )
{
	Cvar_SetLatched( "skill", "1" );
	StartGame();
}

static void HardGameFunc( void *data )
{
	Cvar_SetLatched( "skill", "2" );
	StartGame();
}

static void LoadGameFunc( void *unused )
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc( void *unused )
{
	M_Menu_SaveGame_f();
}

static void CreditsFunc( void *unused )
{
	M_Menu_Credits_f();
}

void Game_MenuInit( void )
{
	static const char *difficulty_names[] =
	{
		"easy",
		"medium",
		"hard",
		0
	};
	float x = SCREEN_WIDTH * 0.50;

	SCR_AdjustSize( &x, NULL, NULL, NULL );

	s_game_menu.x = x;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type = MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x = 0;
	s_easy_game_action.generic.y = 0;
	s_easy_game_action.generic.name = "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type = MTYPE_ACTION;
	s_medium_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x = 0;
	s_medium_game_action.generic.y = 17;
	s_medium_game_action.generic.name	= "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type = MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x = 0;
	s_hard_game_action.generic.y = 34;
	s_hard_game_action.generic.name = "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	s_load_game_action.generic.type = MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x = 0;
	s_load_game_action.generic.y = 68;
	s_load_game_action.generic.name = "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type = MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x = 0;
	s_save_game_action.generic.y = 85;
	s_save_game_action.generic.name = "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x = 0;
	s_credits_action.generic.y = 119;
	s_credits_action.generic.name	= "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem( &s_game_menu, ( void * ) &s_easy_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_medium_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_hard_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_load_game_action );

	if(Host_ServerState())
	{
		// hidden if not a game
		Menu_AddItem( &s_game_menu, ( void * ) &s_save_game_action );
	}

	Menu_AddItem( &s_game_menu, ( void * ) &s_credits_action );
	Menu_Center( &s_game_menu );
}

void Game_MenuDraw( void )
{
	M_Banner( "menu/m_banner_game" );
	Menu_AdjustCursor( &s_game_menu, 1 );
	Menu_Draw( &s_game_menu );
}

const char *Game_MenuKey( int key )
{
	return Default_MenuKey( &s_game_menu, key );
}

void M_Menu_Game_f (void)
{
	Game_MenuInit();
	M_PushMenu( Game_MenuDraw, Game_MenuKey );
	m_game_cursor = 1;
}

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

#define	MAX_SAVEGAMES	15

bool Menu_ReadComment( char *title, int savenum );

static menuframework_s	s_savegame_menu;
static menuframework_s	s_loadgame_menu;
static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];

char m_savestrings[MAX_SAVEGAMES][MAX_QPATH];
bool m_savevalid[MAX_SAVEGAMES];

void Create_Savestrings (void)
{
	int	i;

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		m_savevalid[i] = Menu_ReadComment(m_savestrings[i], i);
	}
}

void LoadGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	if ( m_savevalid[ a->generic.localdata[0] ] )
		Cbuf_AddText(va("load save%i\n",  a->generic.localdata[0] ) );
	M_ForceMenuOff ();
}

void LoadGame_MenuInit( void )
{
	int i;
	float	x = (SCREEN_WIDTH / 2) - 120;
	float	y = (SCREEN_HEIGHT /2) - 58;

	SCR_AdjustSize( &x, &y, NULL, NULL );

	s_loadgame_menu.x = x;
	s_loadgame_menu.y = y;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings();

	for ( i = 0; i < MAX_SAVEGAMES; i++ )
	{
		s_loadgame_actions[i].generic.name = m_savestrings[i];
		s_loadgame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0] = i;
		s_loadgame_actions[i].generic.callback = LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = ( i ) * 17;
		if (i > 0) // separate from autosave
			s_loadgame_actions[i].generic.y += 17;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;
		Menu_AddItem( &s_loadgame_menu, &s_loadgame_actions[i] );
	}
}

void LoadGame_MenuDraw( void )
{
	M_Banner( "menu/m_banner_load_game" );
//	Menu_AdjustCursor( &s_loadgame_menu, 1 );
	Menu_Draw( &s_loadgame_menu );
}

const char *LoadGame_MenuKey( int key )
{
	if ( key == K_ESCAPE || key == K_ENTER )
	{
		s_savegame_menu.cursor = s_loadgame_menu.cursor - 1;
		if ( s_savegame_menu.cursor < 0 ) s_savegame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_loadgame_menu, key );
}

void M_Menu_LoadGame_f (void)
{
	LoadGame_MenuInit();
	M_PushMenu( LoadGame_MenuDraw, LoadGame_MenuKey );
}


/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/
static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];

void SaveGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	Cbuf_AddText (va("save save%i\n", a->generic.localdata[0] ));
	M_ForceMenuOff ();
}

void SaveGame_MenuDraw( void )
{
	M_Banner( "menu/m_banner_save_game" );
	Menu_AdjustCursor( &s_savegame_menu, 1 );
	Menu_Draw( &s_savegame_menu );
}

void SaveGame_MenuInit( void )
{
	int i;
	float	x = (SCREEN_WIDTH / 2) - 120;
	float	y = (SCREEN_HEIGHT /2) - 58;

	SCR_AdjustSize( &x, &y, NULL, NULL );

	s_savegame_menu.x = x;
	s_savegame_menu.y = y;
	s_savegame_menu.nitems = 0;

	Create_Savestrings();

	// don't include the autosave slot
	for ( i = 0; i < MAX_SAVEGAMES - 1; i++ )
	{
		s_savegame_actions[i].generic.name = m_savestrings[i+1];
		s_savegame_actions[i].generic.localdata[0] = i+1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;
		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = ( i ) * 17;
		s_savegame_actions[i].generic.type = MTYPE_ACTION;
		Menu_AddItem( &s_savegame_menu, &s_savegame_actions[i] );
	}
}

const char *SaveGame_MenuKey( int key )
{
	if ( key == K_ENTER || key == K_ESCAPE )
	{
		s_loadgame_menu.cursor = s_savegame_menu.cursor - 1;
		if ( s_loadgame_menu.cursor < 0 )
			s_loadgame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_savegame_menu, key );
}

void M_Menu_SaveGame_f (void)
{
	// not playing a game
	if(!Host_ServerState()) return;

	SaveGame_MenuInit();
	M_PushMenu( SaveGame_MenuDraw, SaveGame_MenuKey );
	Create_Savestrings ();
}


/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
#define MAX_LOCAL_SERVERS 8

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int m_num_servers;
#define	NO_SERVER_STRING	"<no server>"

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

void M_AddToServerList (netadr_t adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS) return;
	while( *info == ' ' ) info++;

	// ignore if duplicated
	for (i = 0; i < m_num_servers; i++)
	{
		if (!std.strcmp(info, local_server_names[i]))
			return;
	}

	local_server_netadr[m_num_servers] = adr;
	std.strncpy (local_server_names[m_num_servers], info, sizeof(local_server_names[0])-1);
	m_num_servers++;
}


void JoinServerFunc( void *self )
{
	char	buffer[128];
	int	index;

	index = (menuaction_s *)self - s_joinserver_server_actions;

	if(!std.stricmp( local_server_names[index], NO_SERVER_STRING ))
		return;
	if(index >= m_num_servers) return;

	std.sprintf(buffer, "connect %s\n", NET_AdrToString(local_server_netadr[index]));
	Cbuf_AddText(buffer);
	M_ForceMenuOff();
}

void AddressBookFunc( void *self )
{
	M_Menu_AddressBook_f();
}

void SearchLocalGames( void )
{
	int	i;

	m_num_servers = 0;
	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
		std.strcpy(local_server_names[i], NO_SERVER_STRING);

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Searching for local servers, this" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );
	
	re->EndFrame();	// the text box won't show up unless we do a buffer swap
	CL_PingServers_f();	// send out info packets
}

void SearchLocalGamesFunc( void *self )
{
	SearchLocalGames();
}

void JoinServer_MenuInit( void )
{
	int	i;
	float	x = SCREEN_WIDTH * 0.50 - 120;

	SCR_AdjustSize( &x, NULL, NULL, NULL );
	s_joinserver_menu.x = x;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name = "address book";
	s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x = 0;
	s_joinserver_address_book_action.generic.y = 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= "refresh server list";
	s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x = 0;
	s_joinserver_search_action.generic.y = 17;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x = 80;
	s_joinserver_server_title.generic.y = 51;

	for( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		s_joinserver_server_actions[i].generic.type = MTYPE_ACTION;
		std.strcpy (local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name = local_server_names[i];
		s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x = 0;
		s_joinserver_server_actions[i].generic.y = 68 + i * 17;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_address_book_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_title );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_action );

	for ( i = 0; i < 8; i++ )
	{
		Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_actions[i] );
	}

	Menu_Center( &s_joinserver_menu );
	SearchLocalGames();
}

void JoinServer_MenuDraw(void)
{
	M_Banner( "menu/m_banner_join_server" );
	Menu_Draw( &s_joinserver_menu );
}


const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( JoinServer_MenuDraw, JoinServer_MenuKey );
}


/*
=============================================================================

START SERVER MENU

=============================================================================
*/
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc( void *self )
{
	if (s_rules_box.curvalue == 1) return;
	M_Menu_DMOptions_f();
}

void RulesChangeFunc ( void *self )
{
	// DM
	if(s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if(s_rules_box.curvalue == 1) // coop
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.field.buffer) > 4)
			strcpy( s_maxclients_field.field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
}

void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int	timelimit;
	int	fraglimit;
	int	maxclients;

	std.strcpy( startmap, strchr( mapnames[s_startmap_list.curvalue], '\n' ) + 1 );
	maxclients = atoi( s_maxclients_field.field.buffer );
	timelimit	= atoi( s_timelimit_field.field.buffer );
	fraglimit	= atoi( s_fraglimit_field.field.buffer );

	Cvar_SetValue("maxclients", ClampCvar( 0, maxclients, maxclients ));
	Cvar_SetValue("timelimit", ClampCvar( 0, timelimit, timelimit ));
	Cvar_SetValue("fraglimit", ClampCvar( 0, fraglimit, fraglimit ));
	Cvar_Set("hostname", s_hostname_field.field.buffer );
	Cvar_SetValue("deathmatch", !s_rules_box.curvalue );
	Cvar_SetValue("coop", s_rules_box.curvalue );

	Cbuf_AddText (va("map %s\n", startmap));
	M_ForceMenuOff();
}

bool Menu_CheckMapsList( void )
{
	byte	buf[MAX_SYSPATH]; // 1 kb
	char	*buffer, string[MAX_STRING];
	search_t	*t;
	file_t	*f;
	int	i;

	if(FS_FileExists("scripts/maps.lst"))
		return true; // exist 

	t = FS_Search( "maps/*.bsp", false );
	if(!t) return false;

	buffer = Z_Malloc( t->numfilenames * 2 * sizeof(string));
	for(i = 0; i < t->numfilenames; i++)
	{
		const char	*data = NULL;
		char		*entities = NULL;
		char		entfilename[MAX_QPATH];
		int		ver = -1, lumpofs = 0, lumplen = 0;
		char		mapname[MAX_QPATH], message[MAX_QPATH];

		f = FS_Open(t->filenames[i], "rb");
		FS_FileBase( t->filenames[i], mapname );

		if( f )
		{
			int num_spawnpoints = 0;

			memset(buf, 0, 1024);
			FS_Read(f, buf, 1024);
			if(!memcmp(buf, "IBSP", 4))
			{
				dheader_t *header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);
				switch(ver)
				{
				case 38:	// quake2 (xash)
				case 46:	// quake3
				case 47:	// return to castle wolfenstein
					lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
					break;
				}
			}
			else
			{
				lump_t	ents; // quake1 entity lump
				memcpy(&ents, buf + 4, sizeof(lump_t)); // skip first four bytes (version)
				ver = LittleLong(((int *)buf)[0]);

				switch( ver )
				{
				case 28:	// quake 1 beta
				case 29:	// quake 1 regular
				case 30:	// Half-Life regular
					lumpofs = LittleLong(ents.fileofs);
					lumplen = LittleLong(ents.filelen);
					break;
				default:
					ver = 0;
					break;
				}
			}
			std.strncpy(entfilename, t->filenames[i], sizeof(entfilename));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			entities = (char *)FS_LoadFile(entfilename, NULL);

			if( !entities && lumplen >= 10 )
			{
				FS_Seek(f, lumpofs, SEEK_SET);
				entities = (char *)Z_Malloc(lumplen + 1);
				FS_Read(f, entities, lumplen);
			}
			if(entities)
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				message[0] = 0;
				data = entities;
				std.strncpy(message, "No Title", MAX_QPATH);

				while(Com_ParseToken(&data))
				{
					if(!strcmp(com_token, "{" )) continue;
					else if(!strcmp(com_token, "}" )) break;
					else if(!strcmp(com_token, "message" ))
					{
						// get the message contents
						Com_ParseToken(&data);
						if(!strcmp(com_token, "" )) continue;
						std.strncpy(message, com_token, sizeof(message));
					}
					else if(!strcmp(com_token, "classname" ))
					{
						Com_ParseToken(&data);
						if(!std.strcmp(com_token, "info_player_deatchmatch"))
							num_spawnpoints++;
						else if(!std.strcmp(com_token, "info_player_start"))
							num_spawnpoints++;
					}
					if(num_spawnpoints > 0) break; // valid map
				}
			}

			if( entities) Z_Free(entities);
			if( f ) FS_Close(f);

			// format: mapname "maptitle"/r
			std.sprintf(string, "%s \"%s\"\r", mapname, message );
			std.strcat(buffer, string); // add new string
		}
	}
	if( t ) Z_Free(t); // free search result

	// write generated maps.lst
	if(FS_WriteFile("scripts/maps.lst", buffer, strlen(buffer)))
	{
          	if( buffer ) Z_Free(buffer);
		return true;
	}
	return false;
}

void StartServer_MenuInit( void )
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};

	char	*s, *buffer;
	float	x = SCREEN_WIDTH * 0.50;
	int	i, length;
	file_t	*fp;

	// create new maplist if not exist
	if(!Menu_CheckMapsList())
	{
		MsgWarn("StartServer_MenuInit: maps.lst not found\n");
		return;
	}

	fp = FS_Open( "scripts/maps.lst", "rb" );
	FS_Seek(fp, 0, SEEK_END);
	length = FS_Tell(fp);
	FS_Seek(fp, 0, SEEK_SET);

	buffer = Z_Malloc( length );
	FS_Read( fp, buffer, length );

	s = buffer;

	i = 0;
	while ( i < length )
	{
		if ( s[i] == '\r' ) nummaps++;
		i++;
	}

	if( nummaps == 0 ) Host_Error( "no maps in maps.lst\n" );
	mapnames = Z_Malloc( sizeof( char * ) * ( nummaps + 1 ) );
	memset( mapnames, 0, sizeof( char * ) * ( nummaps + 1 ) );

	s = buffer;

	for ( i = 0; i < nummaps; i++ )
	{
		char  shortname[MAX_TOKEN_CHARS];
		char  longname[MAX_TOKEN_CHARS];
		char  scratch[200];
		int   j, l;

		std.strncpy( shortname, Com_ParseToken( &s ), MAX_TOKEN_CHARS );
		l = std.strlen(shortname);
		for (j = 0; j < l; j++) shortname[j] = std.toupper(shortname[j]);
		std.strncpy( longname, Com_ParseToken( &s ), MAX_TOKEN_CHARS );
		std.snprintf( scratch, 200, "%s\n%s", longname, shortname );

		mapnames[i] = Z_Malloc( strlen( scratch ) + 1 );
		strcpy( mapnames[i], scratch );
	}

	mapnames[nummaps] = 0;
	if( fp ) FS_Close(fp);
	if( buffer ) Z_Free( buffer );

	// initialize the menu stuff
	SCR_AdjustSize( &x, NULL, NULL, NULL );
	s_startserver_menu.x = x;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x = 0;
	s_startmap_list.generic.y = 0;
	s_startmap_list.generic.name = "initial map";
	s_startmap_list.itemnames = mapnames;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x = 0;
	s_rules_box.generic.y = 34;
	s_rules_box.generic.name = "rules";
	
	s_rules_box.itemnames = dm_coop_names;

	if (Cvar_VariableValue("coop")) s_rules_box.curvalue = 1;
	else s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x = 0;
	s_timelimit_field.generic.y = 59;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.field.maxchars = 3;
	s_timelimit_field.field.widthInChars = 3;
	strcpy( s_timelimit_field.field.buffer, Cvar_VariableString("timelimit") );

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x = 0;
	s_fraglimit_field.generic.y = 84;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.field.maxchars = 3;
	s_fraglimit_field.field.widthInChars = 3;
	strcpy( s_fraglimit_field.field.buffer, Cvar_VariableString("fraglimit") );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is. 
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x = 0;
	s_maxclients_field.generic.y = 109;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.field.maxchars = 3;
	s_maxclients_field.field.widthInChars = 3;
	if( Cvar_VariableValue( "maxclients" ) == 1 )
		strcpy( s_maxclients_field.field.buffer, "8" );
	else strcpy( s_maxclients_field.field.buffer, Cvar_VariableString("maxclients") );

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x = 0;
	s_hostname_field.generic.y = 134;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.field.maxchars = 12;
	s_hostname_field.field.widthInChars = 12;
	strcpy( s_hostname_field.field.buffer, Cvar_VariableString("hostname") );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name = " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x = 24;
	s_startserver_dmoptions_action.generic.y = 159;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= " begin";
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x = 24;
	s_startserver_start_action.generic.y = 176;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );
}

void StartServer_MenuDraw(void)
{
	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key )
{
	if ( key == K_ESCAPE )
	{
		if( mapnames )
		{
			int i;

			for ( i = 0; i < nummaps; i++ )
				Z_Free( mapnames[i] );
			Z_Free( mapnames );
		}
		mapnames = 0;
		nummaps = 0;
	}
	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit();
	M_PushMenu( StartServer_MenuDraw, StartServer_MenuKey );
}

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/
static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;
static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

static void DMFlagCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue( "dmflags" );

	if ( f == &s_friendlyfire_box )
	{
		if ( f->curvalue ) flags &= ~DF_NO_FRIENDLY_FIRE;
		else flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if ( f == &s_falls_box )
	{
		if ( f->curvalue ) flags &= ~DF_NO_FALLING;
		else flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if ( f == &s_weapons_stay_box ) 
	{
		bit = DF_WEAPONS_STAY;
	}
	else if ( f == &s_instant_powerups_box )
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if ( f == &s_allow_exit_box )
	{
		bit = DF_ALLOW_EXIT;
	}
	else if ( f == &s_powerups_box )
	{
		if ( f->curvalue ) flags &= ~DF_NO_ITEMS;
		else flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if ( f == &s_health_box )
	{
		if ( f->curvalue ) flags &= ~DF_NO_HEALTH;
		else flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if ( f == &s_spawn_farthest_box )
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if ( f == &s_teamplay_box )
	{
		if ( f->curvalue == 1 )
		{
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if ( f->curvalue == 2 )
		{
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~( DF_MODELTEAMS | DF_SKINTEAMS );
		}
		goto setvalue;
	}
	else if ( f == &s_samelevel_box )
	{
		bit = DF_SAME_LEVEL;
	}
	else if ( f == &s_force_respawn_box )
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if ( f == &s_armor_box )
	{
		if ( f->curvalue ) flags &= ~DF_NO_ARMOR;
		else flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if ( f == &s_infinite_ammo_box )
	{
		bit = DF_INFINITE_AMMO;
	}
	else if ( f == &s_fixed_fov_box )
	{
		bit = DF_FIXED_FOV;
	}
	else if ( f == &s_quad_drop_box )
	{
		bit = DF_QUAD_DROP;
	}

	if ( f )
	{
		if ( f->curvalue == 0 ) flags &= ~bit;
		else flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", flags);
	sprintf( dmoptions_statusbar, "dmflags = %d", flags );
}

void DMOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] = 
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue( "dmflags" );
	float x = SCREEN_WIDTH * 0.50;
	int y = 0;

	SCR_AdjustSize( &x, NULL, NULL, NULL );
	s_dmoptions_menu.x = x;
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x = 0;
	s_falls_box.generic.y = y;
	s_falls_box.generic.name = "falling damage";
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = ( dmflags & DF_NO_FALLING ) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x = 0;
	s_weapons_stay_box.generic.y = y += 17;
	s_weapons_stay_box.generic.name = "weapons stay";
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = ( dmflags & DF_WEAPONS_STAY ) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x = 0;
	s_instant_powerups_box.generic.y = y += 17;
	s_instant_powerups_box.generic.name = "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = ( dmflags & DF_INSTANT_ITEMS ) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x = 0;
	s_powerups_box.generic.y = y += 17;
	s_powerups_box.generic.name = "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = ( dmflags & DF_NO_ITEMS ) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x = 0;
	s_health_box.generic.y = y += 17;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name = "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = ( dmflags & DF_NO_HEALTH ) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x = 0;
	s_armor_box.generic.y = y += 17;
	s_armor_box.generic.name = "allow armor";
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = ( dmflags & DF_NO_ARMOR ) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x = 0;
	s_spawn_farthest_box.generic.y = y += 17;
	s_spawn_farthest_box.generic.name = "spawn farthest";
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = ( dmflags & DF_SPAWN_FARTHEST ) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x = 0;
	s_samelevel_box.generic.y = y += 17;
	s_samelevel_box.generic.name	= "same map";
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = ( dmflags & DF_SAME_LEVEL ) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x	= 0;
	s_force_respawn_box.generic.y	= y += 17;
	s_force_respawn_box.generic.name = "force respawn";
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = ( dmflags & DF_FORCE_RESPAWN ) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x = 0;
	s_teamplay_box.generic.y = y += 17;
	s_teamplay_box.generic.name	= "teamplay";
	s_teamplay_box.generic.callback = DMFlagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x = 0;
	s_allow_exit_box.generic.y = y += 17;
	s_allow_exit_box.generic.name	= "allow exit";
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = ( dmflags & DF_ALLOW_EXIT ) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x	= 0;
	s_infinite_ammo_box.generic.y	= y += 17;
	s_infinite_ammo_box.generic.name = "infinite ammo";
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = ( dmflags & DF_INFINITE_AMMO ) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x = 0;
	s_fixed_fov_box.generic.y = y += 17;
	s_fixed_fov_box.generic.name	= "fixed FOV";
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = ( dmflags & DF_FIXED_FOV ) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x = 0;
	s_quad_drop_box.generic.y = y += 17;
	s_quad_drop_box.generic.name	= "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = ( dmflags & DF_QUAD_DROP ) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x = 0;
	s_friendlyfire_box.generic.y = y += 17;
	s_friendlyfire_box.generic.name = "friendly fire";
	s_friendlyfire_box.generic.callback = DMFlagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = ( dmflags & DF_NO_FRIENDLY_FIRE ) == 0;

	Menu_AddItem( &s_dmoptions_menu, &s_falls_box );
	Menu_AddItem( &s_dmoptions_menu, &s_weapons_stay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_instant_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_health_box );
	Menu_AddItem( &s_dmoptions_menu, &s_armor_box );
	Menu_AddItem( &s_dmoptions_menu, &s_spawn_farthest_box );
	Menu_AddItem( &s_dmoptions_menu, &s_samelevel_box );
	Menu_AddItem( &s_dmoptions_menu, &s_force_respawn_box );
	Menu_AddItem( &s_dmoptions_menu, &s_teamplay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_allow_exit_box );
	Menu_AddItem( &s_dmoptions_menu, &s_infinite_ammo_box );
	Menu_AddItem( &s_dmoptions_menu, &s_fixed_fov_box );
	Menu_AddItem( &s_dmoptions_menu, &s_quad_drop_box );
	Menu_AddItem( &s_dmoptions_menu, &s_friendlyfire_box );

	Menu_Center( &s_dmoptions_menu );

	// set the original dmflags statusbar
	DMFlagCallback( 0 );
	Menu_SetStatusBar( &s_dmoptions_menu, dmoptions_statusbar );
}

void DMOptions_MenuDraw(void)
{
	Menu_Draw( &s_dmoptions_menu );
}

const char *DMOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_dmoptions_menu, key );
}

void M_Menu_DMOptions_f (void)
{
	DMOptions_MenuInit();
	M_PushMenu( DMOptions_MenuDraw, DMOptions_MenuKey );
}

/*
=============================================================================

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/
static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static void DownloadCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", f->curvalue);
	}

	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", f->curvalue);
	}

	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", f->curvalue);
	}

	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", f->curvalue);
	}

	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", f->curvalue);
	}
}

void DownloadOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	int	y = 0;
	float	x = SCREEN_WIDTH * 0.50;

	SCR_AdjustSize( &x, NULL, NULL, NULL );
	s_downloadoptions_menu.x = x;
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x = 48;
	s_download_title.generic.y = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x = 0;
	s_allow_download_box.generic.y = y += 34;
	s_allow_download_box.generic.name = "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x = 0;
	s_allow_download_maps_box.generic.y = y += 17;
	s_allow_download_maps_box.generic.name	= "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x	= 0;
	s_allow_download_players_box.generic.y = y += 17;
	s_allow_download_players_box.generic.name = "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x = 0;
	s_allow_download_models_box.generic.y = y += 17;
	s_allow_download_models_box.generic.name = "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x = 0;
	s_allow_download_sounds_box.generic.y = y += 17;
	s_allow_download_sounds_box.generic.name = "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem( &s_downloadoptions_menu, &s_download_title );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_maps_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_players_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_models_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_sounds_box );

	Menu_Center( &s_downloadoptions_menu );

	// skip over title
	if (s_downloadoptions_menu.cursor == 0) 
		s_downloadoptions_menu.cursor = 1;
}

void DownloadOptions_MenuDraw(void)
{
	Menu_Draw( &s_downloadoptions_menu );
}

const char *DownloadOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_downloadoptions_menu, key );
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit();
	M_PushMenu( DownloadOptions_MenuDraw, DownloadOptions_MenuKey );
}
/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/
#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

void AddressBook_MenuInit( void )
{
	int	i;

	float	x = (SCREEN_WIDTH / 2) - 142;
	float	y = (SCREEN_HEIGHT /2) - 58;

	SCR_AdjustSize( &x, &y, NULL, NULL );

	s_addressbook_menu.x = x;
	s_addressbook_menu.y = y;
	s_addressbook_menu.nitems = 0;

	for ( i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++ )
	{
		cvar_t *adr;
		char buffer[20];

		sprintf( buffer, "adr%d", i );

		adr = Cvar_Get( buffer, "", CVAR_ARCHIVE );

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x = 0;
		s_addressbook_fields[i].generic.y = i * 25 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].field.cursor = 0;
		s_addressbook_fields[i].field.maxchars = 60;
		s_addressbook_fields[i].field.widthInChars = 30;

		strcpy( s_addressbook_fields[i].field.buffer, adr->string );
		Menu_AddItem( &s_addressbook_menu, &s_addressbook_fields[i] );
	}
}

const char *AddressBook_MenuKey( int key )
{
	if ( key == K_ESCAPE )
	{
		int index;
		char buffer[20];

		for ( index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++ )
		{
			sprintf( buffer, "adr%d", index );
			Cvar_Set( buffer, s_addressbook_fields[index].field.buffer );
		}
	}
	return Default_MenuKey( &s_addressbook_menu, key );
}

void AddressBook_MenuDraw(void)
{
	M_Banner( "menu/m_banner_addressbook" );
	Menu_Draw( &s_addressbook_menu );
}

void M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	M_PushMenu( AddressBook_MenuDraw, AddressBook_MenuKey );
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

void DownloadOptionsFunc( void *self )
{
	M_Menu_DownloadOptions_f();
}

static void HandednessCallback( void *unused )
{
	Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
}

static bool IconOfSkinExists( char *skin, char **pcxfiles, int npcxfiles )
{
	int	i;
	char	scratch[1024];

	strcpy( scratch, skin );
	*strrchr( scratch, '.' ) = 0;
	strncat( scratch, "_i.pcx", 1024 );

	for ( i = 0; i < npcxfiles; i++ )
	{
		if (!strcmp( pcxfiles[i], scratch ))
			return true;
	}

	return false;
}

static bool PlayerConfig_ScanDirectories( void )
{
	search_t	*search;
	char	scratch[1024];
	int	ndirs = 0, npms = 0;
	char	**dirnames;
	int	i;

	s_numplayermodels = 0;
	search = FS_Search( "models/players/*", false );
	if( !search ) return false;
          
          dirnames = search->filenames;
	ndirs = search->numfilenames;

	// go through the subdirectories
	npms = ndirs;
	if ( npms > MAX_PLAYERMODELS ) npms = MAX_PLAYERMODELS;

	for ( i = 0; i < npms; i++ )
	{
		char name[MAX_QPATH];

		if ( dirnames[i] == 0 ) continue;
		if ( strrchr(dirnames[i], '.' )) continue; //skip ".." and "." directories
		
		// verify the existence of player.mdl
		strcpy( scratch, dirnames[i] );
		strncat( scratch, "/player.mdl", 1024 );
		
		if(!FS_FileExists( scratch )) continue;

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = 0;
		s_pmi[s_numplayermodels].skindisplaynames = NULL;
		FS_FileBase(dirnames[i], name );
		strncpy( s_pmi[s_numplayermodels].displayname, name, MAX_DISPLAYNAME - 1 );
		strcpy( s_pmi[s_numplayermodels].directory, name );
		s_numplayermodels++;
	}
	if ( search ) Z_Free( search );

	return true;
}

static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	// sort by male, female, then alphabetical
	if(!strcmp( a->directory, "male" )) return -1;
	else if(!strcmp( b->directory, "male" )) return 1;

	if(!strcmp( a->directory, "female" )) return -1;
	else if(!strcmp( b->directory, "female" )) return 1;

	return strcmp( a->directory, b->directory );
}

bool PlayerConfig_MenuInit( void )
{
	extern cvar_t	*name;
	extern cvar_t	*team;
	extern cvar_t	*skin;
	int		i = 0;
	float		x = SCREEN_WIDTH / 2 - 95;
	float		y = SCREEN_HEIGHT / 2 - 97;

	char currentdirectory[1024];
	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories();
	if (s_numplayermodels == 0) return false;

	if ( hand->value < 0 || hand->value > 2 )
		Cvar_SetValue( "hand", 0 );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if( stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			currentdirectoryindex = i;
			break;
		}
	}

	SCR_AdjustSize( &x, &y, NULL, NULL );
	s_player_config_menu.x = x; 
	s_player_config_menu.y = y;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x	= 0;
	s_player_name_field.generic.y	= 0;
	s_player_name_field.field.maxchars = 20;
	s_player_name_field.field.widthInChars = 20;
	strcpy( s_player_name_field.field.buffer, name->string );
	s_player_name_field.field.cursor = strlen( name->string );

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x = -8;
	s_player_model_title.generic.y = 102;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= 119;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x = 32;
	s_player_hand_title.generic.y	= 187;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x = -56;
	s_player_handedness_box.generic.y = 204;
	s_player_handedness_box.generic.name = 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if(Cvar_VariableValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x = 56;
	s_player_rate_title.generic.y	= 238;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x = -56;
	s_player_rate_box.generic.y = 255;
	s_player_rate_box.generic.name = 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= "download options";
	s_player_download_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x = -24;
	s_player_download_action.generic.y = 272;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_download_action );

	return true;
}

void PlayerConfig_MenuDraw( void )
{
	extern float CalcFov( float fov_x, float w, float h );
	refdef_t	refdef;
	char	scratch[MAX_QPATH];
	float	x = SCREEN_WIDTH / 2;
	float	y = SCREEN_HEIGHT /2 - 72;
	float	w = 144;
	float	h = 168;

	memset( &refdef, 0, sizeof( refdef ) );
	SCR_AdjustSize( &x, &y, &w, &h );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
	refdef.fov_x = 50;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime;

	if ( s_pmi[s_player_model_box.curvalue].directory )
	{
		static int yaw;
		static float frame;
		entity_t entity;

		memset( &entity, 0, sizeof( entity ) );

		sprintf( scratch, "models/players/%s/player.mdl", s_pmi[s_player_model_box.curvalue].directory );
		entity.model = re->RegisterModel( scratch );
		entity.flags = RF_FULLBRIGHT;
		entity.origin[0] = 80;
		entity.origin[1] = 10;
		entity.origin[2] = 0;
		VectorCopy( entity.origin, entity.oldorigin );
		entity.frame = frame += 0.7f;
		entity.sequence = 1;
		entity.prev.frame = 0;
		entity.backlerp = 0.0;
		entity.controller[0] = 90.0;
		entity.controller[1] = 90.0;
		entity.controller[2] = 180.0;
		entity.controller[3] = 180.0;
		entity.angles[1] = 180.0f;

		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( &s_player_config_menu );

		M_DrawTextBox((s_player_config_menu.x / 320.0f), (s_player_config_menu.y / 240.0f), w / 8, h / 8 );
		Msg("x%g y%g\n", (refdef.x) * ( w / 320.0F ) - 148, (refdef.y) * ( h / 240.0F) - 214 );
		refdef.height += 4;

		re->RenderFrame( &refdef );

		strcpy( scratch, "hud/i_fixme" );
		SCR_DrawPic( s_player_config_menu.x - 40, refdef.y, -1, -1, scratch );
	}
}

const char *PlayerConfig_MenuKey (int key)
{
	if ( key == K_ESCAPE )
	{
		Cvar_Set( "name", s_player_name_field.field.buffer );
	}
	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( &s_multiplayer_menu, "No valid player models found" );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	M_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}

/*
====================================================================

VIDEO CONFIG MENU

====================================================================
*/
static menuframework_s	s_video_menu;
static menulist_s		s_mode_list;
static menuslider_s		s_brightness_slider;
static menulist_s  		s_fs_box;
static menulist_s  		s_finish_box;
static menuaction_s		s_cancel_action;
static menuaction_s		s_defaults_action;

extern cvar_t *r_fullscreen;
extern cvar_t *vid_gamma;
static cvar_t *gl_finish;

static void BrightnessCallback( void *s )
{
	menuslider_s *slider = (menuslider_s *)s;
	float gamma = (0.8 - ( slider->curvalue / 10.0 - 0.5 )) + 0.5;

	Cvar_SetValue( "vid_gamma", gamma );
}

static void ResetDefaults( void *unused )
{
	VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
	int needs_restart = 0;

	if(s_fs_box.curvalue != r_fullscreen->value)
		needs_restart++;

	if(s_finish_box.curvalue != gl_finish->value)
		needs_restart++;

	if(s_mode_list.curvalue != Cvar_VariableValue( "r_mode" ))
		needs_restart++;
		
	Cvar_SetValue( "fullscreen", s_fs_box.curvalue );
	Cvar_SetValue( "gl_finish", s_finish_box.curvalue );
	Cvar_SetValue( "r_mode", s_mode_list.curvalue );

	// restart render if needed
	if(needs_restart) Cbuf_AddText("vid_restart\n");

	M_ForceMenuOff();
}

static void CancelChanges( void *unused )
{
	M_PopMenu();
}

/*
** VID_MenuInit
*/
void VID_MenuInit( void )
{
	static const char *resolutions[] = 
	{
		"[640 480  ]",
		"[800 600  ]",
		"[1024 768 ]",
		0,
	};
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0,
	};
	float	x = SCREEN_WIDTH * 0.50;
	float	y = SCREEN_HEIGHT / 2 - 58;

	if ( !gl_finish ) gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE );

	s_mode_list.curvalue = Cvar_VariableValue( "r_mode" );

	SCR_AdjustSize( &x, &y, NULL, NULL );
	s_video_menu.x = x;
	s_video_menu.y = y;
	s_video_menu.nitems = 0;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = 0;
	s_mode_list.itemnames = resolutions;

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.x	= 0;
	s_brightness_slider.generic.y	= 17;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 5;
	s_brightness_slider.maxvalue = 13;
	s_brightness_slider.curvalue = ( 1.3 - vid_gamma->value + 0.5 ) * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x	= 0;
	s_fs_box.generic.y	= 34;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = r_fullscreen->value;

	s_finish_box.generic.type = MTYPE_SPINCONTROL;
	s_finish_box.generic.x = 0;
	s_finish_box.generic.y = 51;
	s_finish_box.generic.name = "sync every frame";
	s_finish_box.curvalue = gl_finish->value;
	s_finish_box.itemnames = yesno_names;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.x = 0;
	s_defaults_action.generic.y = 85;
	s_defaults_action.generic.callback = ResetDefaults;

	s_cancel_action.generic.type = MTYPE_ACTION;
	s_cancel_action.generic.name = "cancel";
	s_cancel_action.generic.x = 0;
	s_cancel_action.generic.y = 102;
	s_cancel_action.generic.callback = CancelChanges;

	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_finish_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_defaults_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_cancel_action );

	Menu_Center( &s_video_menu );
	s_video_menu.x -= 8;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	M_Banner( "menu/m_banner_video" );
	Menu_AdjustCursor( &s_video_menu, 1 );
	Menu_Draw( &s_video_menu );
}

/*
================
VID_MenuKey
================
*/
const char *VID_MenuKey( int key )
{
	menuframework_s *m = &s_video_menu;
	static const char *sound = "misc/menu1.wav";

	switch ( key )
	{
	case K_ESCAPE:
		ApplyChanges( 0 );
		return NULL;
	case K_KP_UPARROW:
	case K_UPARROW:
		m->cursor--;
		Menu_AdjustCursor( m, -1 );
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		m->cursor++;
		Menu_AdjustCursor( m, 1 );
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		Menu_SlideItem( m, -1 );
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		Menu_SlideItem( m, 1 );
		break;
	case K_KP_ENTER:
	case K_ENTER:
		if( !Menu_SelectItem( m ))
			ApplyChanges( NULL );
		break;
	}

	return sound;
}

/*
=======================================================================

QUIT MENU

=======================================================================
*/

const char *M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_PopMenu ();
		break;

	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		break;
	}

	return NULL;

}

void M_Quit_Draw (void)
{
	CG_DrawCenterPic( 320, 240, "background/quit" );
}


void M_Menu_Quit_f (void)
{
	M_PushMenu (M_Quit_Draw, M_Quit_Key);
}



//=============================================================================
/* Menu Subsystem */


/*
=================
M_Init
=================
*/
void M_Init (void)
{
	Cmd_AddCommand ("menu_main", M_Menu_Main_f, "opens main menu" );
	Cmd_AddCommand ("menu_game", M_Menu_Game_f, "opens game menu" );
	Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f, "opens menu loadgame" );
	Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f, "opens menu savegame" );
	Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f, "opens join server menu" );
	Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f, "opens address book menu" );
	Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f, "opens start server menu" );
	Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f, "opens deathmath options menu" );
	Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f, "opens player config menu" );
	Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f, "opens download menu" );
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f, "show credits" );
	Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f, "opens multiplayer menu" );
	Cmd_AddCommand ("menu_video", M_Menu_Video_f, "opens video options menu");
	Cmd_AddCommand ("menu_options", M_Menu_Options_f, "opens main options menu");
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f, "opens redefinition keys menu" );
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f, "show quit dialog" );
}


/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	if (cls.key_dest != key_menu) return;

	m_drawfunc();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		S_StartLocalSound( menu_in_sound );
		m_entersound = false;
	}
}


/*
=================
M_Keydown
=================
*/
void M_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
	{
		if(( s = m_keyfunc( key )) != 0 )
			S_StartLocalSound(( char * )s);
	}
}


