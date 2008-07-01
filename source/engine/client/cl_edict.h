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
	int		serverframe;	// if not current, this ent isn't in the frame

	// cl_private_edict_t
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current;
	entity_state_t	prev;		// will always be valid, but might just be a copy of current
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
	float	realtime;
	float	frametime;
	float	onground;
	float	waterlevel;
	float	clientflags;
	float	intermission;
	float	paused;
	vec3_t	simvel;
	vec3_t	simorg;
	float	idealpitch;
	vec3_t	viewheight;
	vec3_t	cl_viewangles;
	float	health;
	vec3_t	punchangle;
	vec3_t	crosshairangle;
	float	smoothing;
	float	maxclients;
	int	viewentity;
	float	playernum;
	float	demoplayback;
	float	screen_x;
	float	screen_y;
	float	screen_w;
	float	screen_h;
	vec3_t	blend_color;
	float	blend_alpha;
	float	max_entities;
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
	int	chain;
	string_t	model;
};


#define CL_NUM_REQFIELDS (sizeof(cl_reqfields) / sizeof(fields_t))

static fields_t cl_reqfields[] = 
{
	{3,	2,	"flags"}
};

#endif//CL_EDICT_H