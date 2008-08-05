//=======================================================================
//			Copyright XashXT Group 2008 ©
//			cl_edict.h - client prvm edict
//=======================================================================
#ifndef CL_EDICT_H
#define CL_EDICT_H

struct cl_globalvars_s
{
	int	pad[34];
	int	pev;
	int	world;
	string_t	mapname;
	float	realtime;
	float	frametime;
	vec3_t	vieworg;
	vec3_t	viewangles;
	vec3_t	v_forward;
	vec3_t	v_right;
	vec3_t	v_up;
	float	onground;
	float	playernum;
	float	waterlevel;
	float	clientflags;
	vec3_t	cl_viewangles;
	vec3_t	simvel;
	vec3_t	simorg;
	float	idealpitch;
	vec3_t	viewheight;
	float	health;
	float	max_entities;
	float	maxclients;
	float	lerpfrac;
	float	intermission;
	float	demoplayback;
	float	paused;
	vec3_t	punchangle;
	vec3_t	crosshairangle;
	int	viewentity;
	vec3_t	blend_color;
	float	blend_alpha;
	float	screen_x;
	float	screen_y;
	float	screen_w;
	float	screen_h;
	func_t	HUD_Init;
	func_t	HUD_StudioEvent;
	func_t	HUD_ParseMessage;
	func_t	HUD_Render;
	func_t	HUD_UpdateEntities;
	func_t	HUD_Shutdown;
	func_t	V_CalcRefdef;
};

struct cl_entvars_s
{
	string_t	classname;
	string_t	globalname;
	float	modelindex;
	float	soundindex;
	int	chain;
	int	owner;
	string_t	model;
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	mins;
	vec3_t	maxs;
	float	solid;
	float	sequence;
	float	effects;
	float	frame;
	float	body;
	float	skin;
	float	flags;
};

#define PROG_CRC_CLIENT		3720

#endif//CL_EDICT_H