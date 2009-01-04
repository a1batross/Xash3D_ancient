//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       entity_def.h - generic engine edict
//=======================================================================
#ifndef ENTITY_DEF_H
#define ENTITY_DEF_H

typedef struct cl_priv_s	cl_priv_t;	// cl.engine private data
typedef struct sv_priv_s	sv_priv_t;	// sv.engine private data
typedef struct edict_s	edict_t;		// generic entity

// TODO: move to CBaseEntity all fields which doesn't existing on client side
// TODO: generic edict must have all fields as valid on client side too
typedef struct entvars_s
{
	string_t		classname;
	string_t		globalname;
	edict_t		*chain;		// entity pointer when linked into a linked list
	
	vec3_t		origin;
	vec3_t		angles;		// model angles
	int		modelindex;	
	vec3_t		oldorigin;	// interpolated values
	vec3_t		oldangles;

	vec3_t		m_pmatrix[3];	// rotational matrix
	vec3_t		m_pcentre[3];	// physical centre of mass

	vec3_t		velocity;
	vec3_t		avelocity;	// angular velocity (degrees per second)
	vec3_t		movedir;
	vec3_t		force;		// linear physical impulse vector
	vec3_t		torque;		// angular physical impulse vector

	string_t		model;
	string_t		weaponmodel;	// monster or player weaponmodel

	vec3_t		absmin;		// bbox max translated to world coord
	vec3_t		absmax;		// bbox max translated to world coord
	vec3_t		mins;		// local bbox min
	vec3_t		maxs;		// local bbox max
	vec3_t		size;		// maxs - mins
	float		mass;		// physobject mass

	float		ltime;
	float		nextthink;

	int		movetype;
	int		solid;

	int		skin;		//			
	int		body;		// sub-model selection for studiomodels
	int 		effects;
	float		gravity;		// % of "normal" gravity
	float		friction;		// inverse elasticity of MOVETYPE_BOUNCE

	int		sequence;		// animation sequence
	float		frame;		// % playback position in animation sequences (0..255)
	float		animtime;		// world time when frame was set
	float		framerate;	// animation playback rate (-8x to 8x)
	vec3_t		attachment[16];	// server-client attachment actual coords
	float		controller[16];	// bone controller setting (0..255)
	float		blending[16];	// blending amount between sub-sequences (0..255)

	float		scale;		// model rendering scale (0..255)
	int		waterlevel;
	int		watertype;
	int		contents;
	
	float		idealpitch;
	float		pitch_speed;
	float		ideal_yaw;
	float		yaw_speed;

	int		rendermode;
	float		renderamt;
	vec3_t		rendercolor;
	int		renderfx;

	float		health;
	float		frags;
	int		weapons;		// bit mask for available weapons
	float		takedamage;

	int		deadflag;
	vec3_t		view_ofs;		// eye position

	int		button;
	int		impulse;


	edict_t		*dmg_inflictor;
	edict_t		*enemy;
	edict_t		*aiment;		// entity pointer when MOVETYPE_FOLLOW
	edict_t		*owner;
	edict_t		*groundentity;

	int		spawnflags;	// spwanflags are used only during level loading
	int		flags;		// generic flags that can be send to client
	
	// player specific only
	vec3_t		punchangle;	// auto-decaying view angle adjustment
	vec3_t		v_angle;		// viewing angle (player only)
	int		fixangle;		// 0 - nothing, 1 - force view angles, 2 - add avelocity
	string_t		viewmodel;	// player's viewmodel
	int		gaitsequence;	// movement animation sequence for player (0 for none)
	short		colormap;		// lowbyte topcolor, highbyte bottomcolor
	int		playerclass;
	int		team;		// for teamplay
	int		weaponanim;

	float		max_health;
	float		teleport_time;
	int		armortype;
	float		armorvalue;

	string_t		target;
	string_t		targetname;
	string_t		netname;
	string_t		message;

	float		dmg_take;
	float		dmg_save;
	float		dmgtime;
	float		dmg;
	
	string_t		noise;
	string_t		noise1;
	string_t		noise2;
	string_t		noise3;
	string_t		ambient;
	
	float		speed;
	float		air_finished;
	float		pain_finished;
	float		radsuit_finished;
	
	edict_t		*pContainingEntity;		// ptr to class for consistency
} entvars_t;

struct edict_s
{
	BOOL		free;			// shared parms
	float		freetime;			// sv.time when the object was freed
	int		serialnumber;		// must match with entity num

	union
	{
		sv_priv_t	*pvServerData;		// alloced, freed and used by engine only
		cl_priv_t	*pvClientData;		// alloced, freed and used by engine only
	};

	void		*pvPrivateData;		// alloced and freed by engine, used by DLLs
	entvars_t		v;			// C exported fields from progs (network relative)
};

#endif//ENTITY_DEF_H