//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   entity_state.h - a part of network protocol
//=======================================================================
#ifndef ENTITY_STATE_H
#define ENTITY_STATE_H

// engine edict types that shared across network
typedef enum
{
	ED_SPAWNED = 0,	// this entity requris to set own type with SV_ClassifyEdict
	ED_WORLDSPAWN,	// this is a worldspawn
	ED_STATIC,	// this is a logic without model or entity with static model
	ED_AMBIENT,	// this is entity emitted ambient sounds only
	ED_NORMAL,	// normal entity with model (and\or) sound
	ED_BSPBRUSH,	// brush entity (a part of level)
	ED_CLIENT,	// this is a client entity
	ED_MONSTER,	// monster or bot (generic npc with AI)
	ED_TEMPENTITY,	// this edict will be removed on server when "lifetime" exceeds 
	ED_BEAM,		// laser beam (needs to recalculate pvs and frustum)
	ED_MOVER,		// func_train, func_door and another bsp or mdl movers
	ED_VIEWMODEL,	// client or bot viewmodel (for spectating)
	ED_ITEM,		// holdable items
	ED_RAGDOLL,	// dead body with simulated ragdolls
	ED_RIGIDBODY,	// simulated physic
	ED_TRIGGER,	// just for sorting on a server
	ED_PORTAL,	// realtime display, portal or mirror brush or model
	ED_SKYPORTAL,	// realtime 3D-sky camera
	ED_MISSILE,	// greande, rocket e.t.c
	ED_DECAL,		// render will be merge real coords and normal
	ED_VEHICLE,	// controllable vehicle
	ED_MAXTYPES,
} edtype_t;

// entity_state_t->ed_flags
#define ESF_LINKEDICT	BIT( 0 )		// needs to relink edict on client
#define ESF_NODELTA		BIT( 1 )		// force no delta frame
#define ESF_NO_PREDICTION	BIT( 2 )		// e.g. teleport time

typedef struct entity_state_s
{
	// engine specific
	uint		number;		// edict index
	edtype_t		ed_type;		// edict type
	string_t		classname;	// edict classname
	int		soundindex;	// looped ambient sound
	int		ed_flags;		// engine clearing this at end of server frame

	// physics information
	vec3_t		origin;
	vec3_t		angles;		// entity angles, not viewangles
	solid_t		solid;		// entity solid
	movetype_t	movetype;		// entity movetype
	int		gravity;		// gravity multiplier
	int		aiment;		// attached entity
	int		owner;		// projectiles owner
	int		groundent;	// onground edict num, valid only if FL_ONGROUND is set, else -1
	vec3_t		mins;		// not symmetric entity bbox    
	vec3_t		maxs;
	vec3_t		velocity;		// for predicting & tracing
	vec3_t		avelocity;	// for predicting & tracing
	vec3_t		oldorigin;	// portal pvs, lerping state, etc
	int		contents;		// for predicting & tracing on client

	// model state
	int		modelindex;	// general modelindex
	int		colormap;		// change base color for some textures or sprite frames
	float		scale;		// model or sprite scale, affects to physics too
	float		frame;		// % playback position in animation sequences (0..255)
	int		skin;		// skin for studiomodels
	int		body;		// sub-model selection for studiomodels
	float		animtime;		// auto-animating time
	float		framerate;	// custom framerate, specified by QC
	int		sequence;		// animation sequence (0 - 255)
	int		blending[16];	// studio animation blending
	int		controller[16];	// studio bone controllers
	int		flags;		// v.flags
	int		effects;		// effect flags like q1 and hl1
	int		renderfx;		// render effects same as hl1
	float		renderamt;	// alpha value or like somewhat
	vec3_t		rendercolor;	// hl1 legacy stuff, working, but not needed
	int		rendermode;	// hl1 legacy stuff, working, but not needed

	// client specific
	vec3_t		punch_angles;	// add to view direction to get render angles 
	vec3_t		viewangles;	// already calculated view angles on server-side
	vec3_t		viewoffset;	// viewoffset over ground
	int		gaitsequence;	// client\nps\bot gaitsequence
	int		viewmodel;	// contains viewmodel index
	int		weaponmodel;	// contains weaponmodel index
	float		idealpitch;	// client idealpitch
	float		maxspeed;		// min( pev->maxspeed, sv_maxspeed->value )
	float		health;		// client health (other parms can be send by custom messages)
	int		weapons;		// weapon flags
	float		fov;		// horizontal field of view
} entity_state_t;

#endif//ENTITY_STATE_H