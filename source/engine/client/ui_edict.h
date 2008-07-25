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


#define UI_NUM_REQFIELDS (sizeof(ui_reqfields) / sizeof(fields_t))

static fields_t ui_reqfields[] = 
{
	{7,	3,	"click_pos"},
	{10,	3,	"click_size"},
	{13,	2,	"orderpos"},
	{14,	2,	"flag"},
	{15,	2,	"savecount"},
	{16,	2,	"save_valid"},
	{17,	3,	"origin"},
	{20,	6,	"init"},
	{21,	6,	"reinit"},
	{22,	6,	"destroy"},
	{23,	6,	"mouse_enter"},
	{24,	6,	"mouse_leave"},
	{25,	6,	"refresh"},
	{26,	6,	"action"},
	{27,	6,	"draw"},
	{28,	6,	"key"},
	{29,	6,	"_reinit"},
	{30,	6,	"_destroy"},
	{31,	6,	"_mouse_enter"},
	{32,	6,	"_mouse_leave"},
	{33,	6,	"_refresh"},
	{34,	6,	"_action"},
	{35,	6,	"_draw"},
	{36,	6,	"_key"},
	{37,	3,	"color"},
	{40,	2,	"alpha"},
	{41,	1,	"link"},
	{42,	1,	"picture"},
	{43,	3,	"pos"},
	{46,	3,	"size"},
	{49,	1,	"text"},
	{50,	3,	"font_size"},
	{53,	2,	"alignment"},
	{54,	1,	"picture_selected"},
	{55,	1,	"picture_pressed"},
	{56,	1,	"sound_selected"},
	{57,	1,	"sound_pressed"},
	{58,	3,	"color_selected"},
	{61,	3,	"color_pressed"},
	{64,	2,	"alpha_selected"},
	{65,	2,	"alpha_pressed"},
	{66,	2,	"_press_time"},
	{67,	2,	"hold_pressed"},
	{68,	2,	"_button_state"},
	{69,	2,	"style"},
	{70,	1,	"picture_bar"},
	{71,	1,	"sound_changed"},
	{72,	2,	"min_value"},
	{73,	2,	"max_value"},
	{74,	2,	"value"},
	{75,	2,	"step"},
	{76,	3,	"slider_size"},
	{79,	6,	"slidermove"},
	{80,	6,	"switchchange"},
	{81,	1,	"cvarname"},
	{82,	1,	"cvarstring"},
	{83,	2,	"cvartype"},
	{84,	4,	"_link"},
	{85,	2,	"maxlen"},
	{86,	4,	"find1"},
	{87,	2,	"find2"}
};

#define PROG_CRC_UIMENU		2158

#endif//UI_EDICT_H