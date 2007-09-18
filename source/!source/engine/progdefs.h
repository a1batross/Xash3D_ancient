#ifndef PROGDEFS_H
#define PROGDEFS_H

typedef struct globalvars_s
{	
	int 		pad[28];		// parms offsets

	// pointers to ents
	int		pev;		// Pointer EntVars (same as self)
	int		other;
	int		world;

	// timer
	float		time;
	float		frametime;

	// map global info
	string_t		mapname;		// map name
	string_t		startspot;	// landmark name   (new map position - get by name)
	vec3_t		spotoffset;	// landmark offset (old map position)

	// gameplay modes
	float		deathmatch;
	float		coop;
	float		teamplay;

	float		serverflags;	//server flags

	// game info
	float		total_secrets;
	float		total_monsters;
	float		found_secrets;
	float		killed_monsters;

	// AngleVectors result
	vec3_t		v_forward;
	vec3_t		v_right;
	vec3_t		v_up;

	// SV_trace result
	float		trace_allsolid;
	float		trace_startsolid;
	float		trace_fraction;
	vec3_t		trace_endpos;
	vec3_t		trace_plane_normal;
	float		trace_plane_dist;
	float		trace_hitgroup;	// studio model hitgroup number
	float		trace_contents;
	int		trace_ent;
	float		trace_flags;	// misc info

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
	// base entity info
	string_t		classname;
	string_t		globalname;
	float		modelindex;

	// physics description
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;
	vec3_t		post_origin;
	vec3_t		post_angles;
	vec3_t		post_velocity;
	vec3_t		post_avelocity;
	vec3_t		origin_offset;
	vec3_t		angles_offset;
	float		ltime;

	float		bouncetype;
	float		movetype;
	float		solid;
	vec3_t		absmin, absmax;
	vec3_t		mins, maxs;
	vec3_t		size;

	// entity base description
	int		chain;	// dynamic list of all ents
	string_t		model;
	float		frame;
	float		sequence;
	float		renderfx;
	float		effects;
	float		skin;
	float		body;
	string_t		weaponmodel;
	float		weaponframe;
	
	// base generic funcs
	func_t		use;
	func_t		touch;
	func_t		think;
	func_t		blocked;
	func_t		activate;

	// npc generic funcs
	func_t		walk;
	func_t		jump;
	func_t		duck;

	// flags
	float		flags;
	float		aiflags;
	float		spawnflags;
				
	// other variables
	int		groundentity;
	float		nextthink;
	float		takedamage;
	float		health;

	float		frags;
	float		weapon;
	float		items;
	string_t		target;
	string_t		parent;
	string_t		targetname;
	int		aiment;		// attachment edict
	int		goalentity;
	vec3_t		punchangle;	
	float		deadflag;
	vec3_t		view_ofs;		//int		viewheight;
	float		button0;
	float		button1;
	float		button2;
	float		impulse;
	float		fixangle;
	vec3_t		v_angle;
	float		idealpitch;
	string_t		netname;
	int		enemy;
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
	float		jumpup;
	float		jumpdn;
	int		movetarget;
	float		mass;
	float		density;
	float		gravity;
	float		dmg;
	float		dmgtime;
	float		speed;

} entvars_t;

#define PROG_CRC_SERVER 42175

#endif//PROGDEFS_H