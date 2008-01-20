//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pmove.h - base player physic
//=======================================================================
#ifndef COLLISION_H
#define COLLISION_H

// encoded bmodel mask
#define SOLID_BMODEL	0xffffff

// content masks
#define MASK_ALL		(-1)
#define MASK_SOLID		(CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID	(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER		(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE		(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT		(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT	(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)

// pmove_state_t is the information necessary for client side movement
#define PM_NORMAL			0 // can accelerate and turn
#define PM_SPECTATOR		1
#define PM_DEAD			2 // no acceleration or turning
#define PM_GIB			3 // different bounding box
#define PM_FREEZE			4
#define PM_INTERMISSION		5
#define PM_NOCLIP			6

// pmove->pm_flags
#define PMF_DUCKED			1
#define PMF_JUMP_HELD		2
#define PMF_ON_GROUND		4
#define PMF_TIME_WATERJUMP		8	// pm_time is waterjump
#define PMF_TIME_LAND		16	// pm_time is time before rejump
#define PMF_TIME_TELEPORT		32	// pm_time is non-moving time
#define PMF_NO_PREDICTION		64	// temporarily disables prediction (used for grappling hook)
#define PMF_ALL_TIMES		(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_TELEPORT)

// viewmodel state
typedef struct
{
	int		index;	// client modelindex
	vec3_t		angles;	// can be some different with viewangles
	vec3_t		offset;	// center offset
	int		sequence;	// studio animation sequence
	int		frame;	// studio frame
	int		body;	// weapon body
	int		skin;	// weapon skin 
} vmodel_state_t;

// thirdperson model state
typedef struct
{
	int		index;	// client modelindex
	int		sequence;	// studio animation sequence
	int		frame;	// studio frame
} pmodel_state_t;

// player_state_t communication
typedef struct
{
	int		bobcycle;		// for view bobbing and footstep generation
	float		bobtime;
	byte		pm_type;		// player movetype
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms

	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		delta_angles;	// add to command angles to get view direction
	short		gravity;		// gravity value
	short		speed;		// maxspeed
	edict_t		*groundentity;	// current ground entity
	int		viewheight;	// height over ground
	int		effects;		// copied to entity_state_t->effects
	int		weapon;		// copied to entity_state_t->weapon
	vec3_t		viewangles;	// for fixed views
	vec3_t		viewoffset;	// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
	vec3_t		oldviewangles;	// for lerping viewmodel position
	vec4_t		blend;		// rgba full screen effect
	short		stats[MAX_STATS];
	float		fov;		// horizontal field of view

	// player model and viewmodel
	vmodel_state_t	vmodel;
	pmodel_state_t	pmodel;

} player_state_t;

// user_cmd_t communication
typedef struct usercmd_s
{
	byte		msec;
	byte		buttons;
	short		angles[3];
	byte		impulse;		// remove?
	byte		lightlevel;	// light level the player is standing on
	short		forwardmove, sidemove, upmove;
} usercmd_t;

#define MAXTOUCH		32
typedef struct
{
	player_state_t	ps;		// state (in / out)

	// command (in)
	usercmd_t		cmd;

	// results (out)
	int		numtouch;
	edict_t		*touchents[MAXTOUCH];

	vec3_t		mins, maxs;	// bounding box size
	int		watertype;
	int		waterlevel;
	float		xyspeed;		// avoid to compute it twice

	// callbacks to test the world
	trace_t		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end );
	int		(*pointcontents)( vec3_t point );
} pmove_t;

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/
// button bits
#define	BUTTON_ATTACK		1
#define	BUTTON_USE		2
#define	BUTTON_ATTACK2		4
#define	BUTTONS_ATTACK		(BUTTON_ATTACK | BUTTON_ATTACK2)
#define	BUTTON_ANY		128 // any key whatsoever

extern float pm_airaccelerate;
void Pmove( pmove_t *pmove );

#endif//COLLISION_H