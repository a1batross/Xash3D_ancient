//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       entity_def.h - generic engine edict
//=======================================================================
#ifndef ENTITY_DEF_H
#define ENTITY_DEF_H

// Legend:
// ENG - engine can modify this variable for some reasons [only
// NET - field that shared on client across network
// Modifiers:
// [player] - all notify for this field is valid only for client entity
// [all] - valid for all ents
// [phys] - valid only for rigid bodies
// [solid] - only for solid entities
// [push] - only ents with SOLID_BSP and MOVETYPE_PUSH have affect on this field

typedef struct entvars_s
{
	string_t		classname;	// ENG [all], NET [all]
	string_t		globalname;	// global entity name transmitted across levels
	
	vec3_t		origin;		// ENG [all], NET [all]
	vec3_t		oldorigin;	// ENG [all], NET [all]
	vec3_t		velocity;
	vec3_t		basevelocity;
	vec3_t		clbasevelocity;	// ENG [player], NET [player]

	vec3_t		movedir;

	vec3_t		angles;		// ENG [all], NET [all]
	vec3_t		avelocity;	// angular velocity (degrees per second)
	vec3_t		punchangle;	// NET [player], auto-decaying view angle adjustment
	vec3_t		viewangles;	// NET [player], viewing angle (old name was v_angle)

	int		fixangle;		// 0 - nothing, 1 - force view angles, 2 - add avelocity	
	float		ideal_pitch;
	float		pitch_speed;
	float		ideal_yaw;
	float		yaw_speed;

	int		modelindex;	// ENG [all], NET [all]
	int		soundindex;	// ENG [all], NET [all]

	string_t		model;		// model name
	string_t		viewmodel;	// player's viewmodel (no network updates)
	string_t		weaponmodel;	// NET [all] - sending weaponmodel index, not name

	vec3_t		absmin;		// ENG [all] - pfnSetAbsBox passed to modify this values
	vec3_t		absmax;		// ENG [all] - pfnSetAbsBox passed to modify this values 
	vec3_t		mins;		// ENG [all], NET [solid]
	vec3_t		maxs;		// ENG [all], NET [solid]
	vec3_t		size;		// ENG [all], restored on client-side from mins-maxs 

	float		ltime;		// [push]
	float		nextthink;	// time to next call of think function

	int		movetype;		// ENG [all], NET [all]
	int		solid;		// ENG [all], NET [all]

	int		skin;		// NET [all]			
	int		body;		// NET [all], sub-model selection for studiomodels
	int		weaponbody;	// NET [all], sub-model selection for weaponmodel
	int		weaponskin;	// NET [all],
	int 		effects;		// ENG [all], NET [all]
	float		gravity;		// % of "normal" gravity
	float		friction;		// inverse elasticity of MOVETYPE_BOUNCE
	float		speed;
	float		mass;		// [phys] physic mass

	int		light_level;	// entity current lightlevel

	int		sequence;		// ENG [all], NET [all], animation sequence
	int		gaitsequence;	// NET [player], movement animation sequence for player (0 for none)
	float		frame;		// NET [all], % playback position in animation sequences (0..255)
	float		animtime;		// NET [all], world time when frame was set
	float		framerate;	// NET [all], animation playback rate (-8x to 8x)
	byte		controller[16];	// NET [all], bone controller setting (0..255)
	byte		blending[16];	// NET [all], blending amount between sub-sequences (0..255)

	float		scale;		// NET [all], sprites and models rendering scale (0..255)
	int		rendermode;	// NET [all]
	float		renderamt;	// NET [all]
	vec3_t		rendercolor;	// NET [all]
	int		renderfx;		// NET [all]

	float		fov;		// NET [player], client fov, used instead m_iFov
	float		health;		// NET [player]
	float		frags;
	int		weapons;		// NET [player], bit mask for available weapons
	int		items;		// from Q1, can use for holdable items or user flags 
	float		takedamage;
	float		maxspeed;		// NET [player], uses to limit speed for current client

	int		deadflag;
	vec3_t		view_ofs;		// NET [player], eye position

	int		button;
	int		impulse;

	edict_t		*chain;		// linked list for EntitiesInPHS\PVS
	edict_t		*dmg_inflictor;
	edict_t		*enemy;
	edict_t		*aiment;		// NET [all], entity pointer when MOVETYPE_FOLLOW
	edict_t		*owner;		// NET [all]
	edict_t		*groundentity;	// NET [all], only if FL_ONGROUND is set

	int		spawnflags;	// spwanflags are used only during level loading
	int		flags;		// generic flags that can be send to client

	short		colormap;		// lowbyte topcolor, highbyte bottomcolor
	int		team;		// ENG [player], NET [player], for teamplay

	float		max_health;
	float		teleport_time;	// ENG [all], NET [all], engine will be reset value on next frame
	int		armortype;
	float		armorvalue;
	int		waterlevel;	// ENG [all]
	int		watertype;	// ENG [all]
	int		contents;		// hl-coders: use this instead of pev->skin, to set entity contents

	string_t		target;		// various server strings
	string_t		targetname;
	string_t		netname;
	string_t		message;
	string_t		noise;
	string_t		noise1;
	string_t		noise2;
	string_t		noise3;

	float		dmg_take;
	float		dmg_save;
	float		dmg;
	float		dmgtime;

	edict_t		*pContainingEntity;		// filled by engine, don't save, don't modifiy

	// pm_shared test stuff
	int		bInDuck;
	int		flTimeStepSound;	// Next time we can play a step sound
	int		flSwimTime;         // In process of ducking or ducked already?
	int		flDuckTime;	// Time we started duck
	int		iStepLeft;         	// 0 - 4
	float		flJumpPadTime;	// for scale falling damage
	float		flFallVelocity;	// falling velocity z
	int		oldbuttons;	// buttons last usercmd
	int		groupinfo;	// entities culling (on server)
	int		iSpecMode;	// OBS_ROAMING etc (was iuser1)

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