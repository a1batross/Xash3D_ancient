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
#include "progsvm.h"
#include "vm_cmds.h"

#define M_PROG_FILENAME	"menu.dat"
#define M_NAME		"menu"
#define M_MAX_EDICTS	(1 << 12) // should be enough for a menu

enum m_state_e {
	m_none,
	m_main,
	m_demo,
	m_singleplayer,
	m_transfusion_episode,
	m_transfusion_skill,
	m_load,
	m_save,
	m_multiplayer,
	m_setup,
	m_options,
	m_video,
	m_keys,
	m_help,
	m_credits,
	m_quit,
	m_lanconfig,
	m_gameoptions,
	m_slist,
	m_options_effects,
	m_options_graphics,
	m_options_colorcontrol,
	m_reset
};
extern enum m_state_e m_state;

#endif//UIMENU_H