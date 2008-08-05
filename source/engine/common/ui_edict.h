//=======================================================================
//			Copyright XashXT Group 2008 ©
//			ui_edict.h - uimenu prvm edict
//=======================================================================
#ifndef UI_EDICT_H
#define UI_EDICT_H

struct ui_globalvars_s
{
	int	pad[34];
	int	pev;
	float	time;
	func_t	m_init;
	func_t	m_shutdown;
	func_t	m_show;
	func_t	m_hide;
	func_t	m_draw;
	func_t	m_endofcredits;
	func_t	m_keydown;
};

struct ui_entvars_s
{
	string_t	name;
	string_t	type;
	string_t	parent;
	int	_parent;
	int	_child;
	int	_next;
	int	_prev;
};

#define PROG_CRC_UIMENU		2158

#endif//UI_EDICT_H