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

typedef struct cl_globalvars_s
{
	int	pad[28];
	int	pev;
	int	other;
	int	world;
	float	time;
	float	frametime;
	float	player_localentnum;
	float	player_localnum;
	float	maxclients;
	float	clientcommandframe;
	float	servercommandframe;
	string_t	mapname;
	vec3_t	v_forward;
	vec3_t	v_up;
	vec3_t	v_right;
	float	scr_width;
	float	scr_height;
	float	x_pos;
	float	y_pos;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_plane_dist;
	float	trace_hitgroup;
	float	trace_contents;
	int	trace_ent;
	float	trace_flags;
	func_t	main;
	func_t	ClientInit;
	func_t	ClientFree;
	func_t	ClientPreThink;
	func_t	ClientPostThink;
	func_t	UpdateView;
	func_t	ConsoleCommand;
	vec3_t	pmove_org;
	vec3_t	pmove_vel;
	vec3_t	pmove_mins;
	vec3_t	pmove_maxs;
} cl_globalvars_t;

typedef struct cl_entvars_s
{
	float	modelindex;
	vec3_t	absmin;
	vec3_t	absmax;
	float	entnum;
	float	drawmask;
	func_t	predraw;
	float	movetype;
	float	solid;
	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	velocity;
	vec3_t	angles;
	vec3_t	avelocity;
	string_t	classname;
	string_t	model;
	float	frame;
	float	skin;
	float	effects;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	float	nextthink;
	int	chain;
	string_t	netname;
	float	flags;
	float	alpha;
	float	renderflags;
	float	scale;
} cl_entvars_t;


#define CL_NUM_REQFIELDS (sizeof(cl_reqfields) / sizeof(fields_t))

static fields_t cl_reqfields[] = 
{
	{48,	1,	"message"},
	{49,	2,	"enttype"},
	{50,	2,	"sv_entnum"}
};

#define PROG_CRC_CLIENT	7430

#endif//CL_EDICT_H