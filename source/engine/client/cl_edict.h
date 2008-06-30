//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_edict.h - client prvm edict
//=======================================================================
#ifndef CL_EDICT_H
#define CL_EDICT_H

struct cl_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool		free;
	float		freetime;	 	// cl.time when the object was freed

	// ui_private_edict_t starts here
};

struct cl_globalvars_s
{
	int	pad[28];
	int	pev;
	int	world;
	string_t	mapname;
	vec3_t	vieworg;
	vec3_t	viewangles;
	vec3_t	v_forward;
	vec3_t	v_right;
	vec3_t	v_up;
	float	time;
	float	frametime;
	float	intermission;
	float	paused;
	float	spectator;
	float	onground;
	float	waterlevel;
	vec3_t	simvel;
	vec3_t	simorg;
	vec3_t	viewheight;
	float	idealpitch;
	vec3_t	cl_viewangles;
	float	health;
	vec3_t	crosshairangle;
	vec3_t	punchangle;
	float	maxclients;
	int	viewentity;
	float	playernum;
	float	max_entities;
	float	demoplayback;
	float	smoothing;
	float	screen_x;
	float	screen_y;
	float	screen_w;
	float	screen_h;
	vec3_t	blend_color;
	float	blend_alpha;
	func_t	HUD_Init;
	func_t	HUD_Render;
	func_t	HUD_ConsoleCommand;
	func_t	HUD_ParseMessage;
	func_t	HUD_Shutdown;
	func_t	V_CalcRefdef;
};

struct cl_entvars_s
{
	string_t	classname;
	string_t	model;
	int	chain;
	float	frame;
	vec3_t	origin;
	vec3_t	angles;
	float	sequence;
	float	animtime;
	float	framerate;
	float	alpha;
	float	body;
	float	skin;
	float	effects;
	float	renderflags;
};


#define CL_NUM_REQFIELDS (sizeof(cl_reqfields) / sizeof(fields_t))

static fields_t cl_reqfields[] = 
{
	{18,	2,	"flags"}
};

#endif//CL_EDICT_H