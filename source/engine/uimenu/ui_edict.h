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
	func_t		m_shutdown;
	func_t		m_keydown;
	func_t		m_draw;
	func_t		m_show;
	func_t		m_hide;
};

struct ui_entvars_s
{
	string_t		type;
	string_t		parent;
	string_t		name;
	int		_parent;
	int		_next;
	int		_prev;
	int		_child;
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
	{41,	3,	"color"},
	{44,	2,	"alpha"},
	{45,	2,	"drawflag"},
	{46,	1,	"link"},
	{47,	1,	"picture"},
	{48,	3,	"pos"},
	{51,	3,	"size"},
	{54,	1,	"text"},
	{55,	3,	"font_size"},
	{58,	2,	"alignment"},
	{59,	1,	"picture_selected"},
	{60,	1,	"picture_pressed"},
	{61,	1,	"sound_selected"},
	{62,	1,	"sound_pressed"},
	{63,	3,	"color_selected"},
	{66,	3,	"color_pressed"},
	{69,	2,	"alpha_selected"},
	{70,	2,	"alpha_pressed"},
	{71,	2,	"drawflag_selected"},
	{72,	2,	"drawflag_pressed"},
	{73,	2,	"_press_time"},
	{74,	2,	"hold_pressed"},
	{75,	2,	"_button_state"},
	{76,	2,	"style"},
	{77,	1,	"picture_bar"},
	{78,	1,	"sound_changed"},
	{79,	2,	"min_value"},
	{80,	2,	"max_value"},
	{81,	2,	"value"},
	{82,	2,	"step"},
	{83,	3,	"slider_size"},
	{86,	6,	"slidermove"},
	{87,	6,	"switchchange"},
	{88,	1,	"cvarname"},
	{89,	2,	"cvartype"},
	{90,	4,	"_link"},
	{91,	2,	"maxlen"},
	{92,	4,	"find1"},
	{93,	2,	"find2"}
};

#endif//UI_EDICT_H