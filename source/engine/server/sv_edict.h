//=======================================================================
//			Copyright XashXT Group 2008 ©
//			sv_edict.h - server prvm edict
//=======================================================================
#ifndef SV_EDICT_H
#define SV_EDICT_H

struct sv_globalvars_s
{
	int	pad[34];
	int	pev;
	int	other;
	int	world;
	float	time;
	float	frametime;
	float	serverflags;
	string_t	mapname;
	string_t	startspot;
	vec3_t	spotoffset;
	float	deathmatch;
	float	teamplay;
	float	coop;
	float	total_secrets;
	float	found_secrets;
	float	total_monsters;
	float	killed_monsters;
	vec3_t	v_forward;
	vec3_t	v_right;
	vec3_t	v_up;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	float	trace_plane_dist;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_contents;
	float	trace_hitgroup;
	float	trace_flags;
	int	trace_ent;
	func_t	CreateAPI;
	func_t	StartFrame;
	func_t	EndFrame;
	func_t	PlayerPreThink;
	func_t	PlayerPostThink;
	func_t	ClientConnect;
	func_t	ClientDisconnect;
	func_t	PutClientInServer;
	func_t	ClientCommand;
	func_t	ClientUserInfoChanged;
};

struct sv_entvars_s
{
	string_t	classname;
	string_t	globalname;
	int	chain;
	func_t	precache;
	func_t	activate;
	func_t	blocked;
	func_t	touch;
	func_t	think;
	func_t	use;
	vec3_t	origin;
	vec3_t	angles;
	float	modelindex;
	vec3_t	old_origin;
	vec3_t	old_angles;
	vec3_t	velocity;
	vec3_t	avelocity;
	vec3_t	m_pcentre[3];
	vec3_t	m_pmatrix[4];
	vec3_t	movedir;
	vec3_t	force;
	vec3_t	torque;
	vec3_t	post_origin;
	vec3_t	post_angles;
	vec3_t	origin_offset;
	vec3_t	absmin;
	vec3_t	absmax;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	float	mass;
	float	solid;
	float	movetype;
	float	bouncetype;
	float	waterlevel;
	float	watertype;
	float	ltime;
	string_t	model;
	float	skin;
	float	body;
	float	alpha;
	float	frame;
	float	speed;
	float	animtime;
	float	sequence;
	float	effects;
	float	colormap;
	float	renderfx;
	float	flags;
	float	aiflags;
	float	spawnflags;
	vec3_t	view_ofs;
	vec3_t	v_angle;
	float	button0;
	float	button1;
	float	button2;
	float	impulse;
	vec3_t	punchangle;
	string_t	v_model;
	float	v_frame;
	float	v_body;
	float	v_skin;
	float	v_sequence;
	string_t	p_model;
	float	p_frame;
	float	p_body;
	float	p_skin;
	float	p_sequence;
	string_t	loopsound;
	float	loopsndvol;
	float	loopsndattn;
	int	owner;
	int	enemy;
	int	aiment;
	int	goalentity;
	float	fixangle;
	float	ideal_yaw;
	float	yaw_speed;
	float	teleport_time;
	int	groundentity;
	float	takedamage;
	float	nextthink;
	float	health;
	float	gravity;
	float	frags;
	float	team;
};

#define PROG_CRC_SERVER		1476

#endif//SV_EDICT_H