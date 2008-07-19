//=======================================================================
//			Copyright XashXT Group 2007 ©
//			ui_edict.h - uimenu prvm edict
//=======================================================================
#ifndef UI_EDICT_H
#define UI_EDICT_H

struct ui_globalvars_s
{
	int		pad[28];
	int		pev;
	func_t		m_init;
	func_t		m_show;
	func_t		m_draw;
	func_t		m_hide;
	func_t		m_endofcredits;
	func_t		m_keydown;
	func_t		m_shutdown;
	float		time;
};

struct ui_entvars_s
{
	string_t		name;
	string_t		parent;
	string_t		type;
	int		_parent;
	int		_child;
	int		_next;
	int		_prev;
};

struct ui_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool		free;
	float		freetime;	 	// sv.time when the object was freed

	// ui_private_edict_t starts here
};

#define UI_NUM_REQFIELDS (sizeof(ui_reqfields) / sizeof(fields_t))

static fields_t ui_reqfields[] = 
{
	{7,	3,	"click_pos"},
	{10,	3,	"click_size"},
	{13,	2,	"orderpos"},
	{14,	2,	"flag"},
	{15,	3,	"clip_pos"},
	{18,	3,	"clip_size"},
	{21,	3,	"origin"},
	{24,	6,	"init"},
	{25,	6,	"reinit"},
	{26,	6,	"destroy"},
	{27,	6,	"mouse_enter"},
	{28,	6,	"mouse_leave"},
	{29,	6,	"refresh"},
	{30,	6,	"action"},
	{31,	6,	"draw"},
	{32,	6,	"key"},
	{33,	6,	"_reinit"},
	{34,	6,	"_destroy"},
	{35,	6,	"_mouse_enter"},
	{36,	6,	"_mouse_leave"},
	{37,	6,	"_refresh"},
	{38,	6,	"_action"},
	{39,	6,	"_draw"},
	{40,	6,	"_key"},
	{41,	2,	"savecount"},
	{42,	2,	"save_valid"},
	{43,	3,	"color"},
	{46,	2,	"alpha"},
	{47,	1,	"link"},
	{48,	1,	"picture"},
	{49,	3,	"pos"},
	{52,	3,	"size"},
	{55,	1,	"text"},
	{56,	3,	"font_size"},
	{59,	2,	"alignment"},
	{60,	1,	"picture_selected"},
	{61,	1,	"picture_pressed"},
	{62,	1,	"sound_selected"},
	{63,	1,	"sound_pressed"},
	{64,	3,	"color_selected"},
	{67,	3,	"color_pressed"},
	{70,	2,	"alpha_selected"},
	{71,	2,	"alpha_pressed"},
	{72,	2,	"_press_time"},
	{73,	2,	"hold_pressed"},
	{74,	2,	"_button_state"},
	{75,	2,	"style"},
	{76,	1,	"picture_bar"},
	{77,	1,	"sound_changed"},
	{78,	2,	"min_value"},
	{79,	2,	"max_value"},
	{80,	2,	"value"},
	{81,	2,	"step"},
	{82,	3,	"slider_size"},
	{85,	6,	"slidermove"},
	{86,	6,	"switchchange"},
	{87,	1,	"cvarname"},
	{88,	1,	"cvarstring"},
	{89,	2,	"cvartype"},
	{90,	4,	"_link"},
	{91,	2,	"maxlen"},
	{92,	4,	"find1"},
	{93,	2,	"find2"}
};

#define PROG_CRC_UIMENU		2460

#endif//UI_EDICT_H