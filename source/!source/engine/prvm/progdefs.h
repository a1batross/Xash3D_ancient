#ifndef PROGDEFS_H
#define PROGDEFS_H

typedef struct globalvars_s
{	
	int 		pad[28];
	int		self;
	int		other;
	int		world;
	float		time;
	float		frametime;
	float		force_retouch;
	string_t		mapname;
	float		deathmatch;
	float		coop;
	float		teamplay;
	float		serverflags;
	float		total_secrets;
	float		total_monsters;
	float		found_secrets;
	float		killed_monsters;
	float		parm[16];
	vec3_t		v_forward;
	vec3_t		v_up;
	vec3_t		v_right;
	float		trace_allsolid;
	float		trace_startsolid;
	float		trace_fraction;
	vec3_t		trace_endpos;
	vec3_t		trace_plane_normal;
	float		trace_plane_dist;
	int		trace_ent;
	float		trace_inopen;
	float		trace_inwater;
	int		msg_entity;

	// game_export_s
	func_t		main;		// Init
	func_t		StartFrame;	// RunFrame
	func_t		EndFrame;		// EndFrame
	func_t		PlayerPreThink;	// ClientThink
	func_t		PlayerPostThink;	// ClientThink
	func_t		ClientKill;	// ???
	func_t		ClientConnect;	// ClientConnect
	func_t		PutClientInServer;	// ClientBegin
	func_t		ClientDisconnect;	// ClientDisconnect
	func_t		SetNewParms;	// ???
	func_t		SetChangeParms;	// ???

} globalvars_t;

typedef struct entvars_s
{
	float		modelindex;
	vec3_t		absmin;
	vec3_t		absmax;
	float		ltime;
	float		movetype;
	float		solid;
	vec3_t		origin;
	vec3_t		oldorigin;
	vec3_t		velocity;
	vec3_t		angles;
	vec3_t		avelocity;
	vec3_t		punchangle;
	string_t		classname;
	string_t		model;
	float		frame;
	float		skin;
	float		body;
	float		effects;
	float		sequence;
	float		renderfx;
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		size;
	func_t		touch;
	func_t		use;
	func_t		think;
	func_t		blocked;
	float		nextthink;
	int		groundentity;
	float		health;
	float		frags;
	float		weapon;
	string_t		weaponmodel;
	float		weaponframe;
	float		currentammo;
	float		ammo_shells;
	float		ammo_nails;
	float		ammo_rockets;
	float		ammo_cells;
	float		items;
	float		takedamage;
	int		chain;
	float		deadflag;
	vec3_t		view_ofs;
	float		button0;
	float		button1;
	float		button2;
	float		impulse;
	float		fixangle;
	vec3_t		v_angle;
	float		idealpitch;
	string_t		netname;
	int		enemy;
	float		flags;
	float		colormap;
	float		team;
	float		max_health;
	float		teleport_time;
	float		armortype;
	float		armorvalue;
	float		waterlevel;
	float		watertype;
	float		ideal_yaw;
	float		yaw_speed;
	int		aiment;
	int		goalentity;
	float		spawnflags;
	string_t		target;
	string_t		targetname;
	float		dmg_take;
	float		dmg_save;
	int		dmg_inflictor;
	int		owner;
	vec3_t		movedir;
	string_t		message;
	float		sounds;
	string_t		noise;
	string_t		noise1;
	string_t		noise2;
	string_t		noise3;

} entvars_t;

#define PROG_CRC_SERVER 21645

#endif//PROGDEFS_H