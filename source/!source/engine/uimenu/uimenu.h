/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#ifndef UIMENU_H
#define UIMENU_H

#include "engine.h"
#include "client.h"
#include "qmenu.h"
#include "progsvm.h"

#define UI_MAX_EDICTS	(1 << 12) // should be enough for a menu

extern bool ui_active;
extern const int vm_ui_numbuiltins;
extern prvm_builtin_t vm_ui_builtins[];

void UI_Init( void );
void UI_KeyEvent( int key );
void UI_ToggleMenu_f( void );
void UI_Shutdown( void );
void UI_Draw( void );

#endif//UIMENU_H