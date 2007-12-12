/*
+----+
|Defs|
+----+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| This contains necessary definitions from the original V1.06 defs.qc file.  |
| This includes some basic constants, the built in function definitions, and |
| some variable's used by the Quake Engine internally.                       |
| Certain lines in this file are hardcoded into Quake engine, and -must- be  |
| present and unchanged, in the order they are shown. Otherwise Quake will   |
| refuse to run.                                                             |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/
#pragma version 7		// set right version

// These lines CANNOT be altered/moved
	// pointers to ents
	entity          	pev;		// Pointer EntVars (same as pev)
	entity		other;
	entity		world;
 
	// timer
	float		time;
	float		frametime;

	// map global info
	string		mapname;
	string		startspot;
	vector		spotoffset;

	// gameplay modes
	float		deathmatch;
	float		coop;
	float		teamplay;

	float		serverflags;	// propagated from level to level, used to

	// game info
	float		total_secrets;
	float		total_monsters;
	float		found_secrets;	// number of secrets found
	float		killed_monsters;	// number of monsters killed

	// MakeVectors result
	vector		v_forward;
	vector              v_right;
	vector		v_up;

	// SV_trace result
	float		trace_allsolid;
	float		trace_startsolid;
	float		trace_fraction;
	vector		trace_endpos;
	vector		trace_plane_normal;
	float		trace_plane_dist;
	float		trace_hitgroup;
	float		trace_contents;
	entity		trace_ent;
	float		trace_flags;

	void()          	main;                            // only for testing
	void()		StartFrame;
	void()		EndFrame;
	void() 		PlayerPreThink;
	void() 		PlayerPostThink;
	void()		ClientKill;
	void()		ClientConnect;
	void() 		PutClientInServer;		// call after setting the parm1... parms
	void()		ClientDisconnect;
	void()		ClientCommand;		// process client commands

void end_sys_globals;		// flag for structure dumping

	// base entity info
	.string		classname;
	.string		globalname;
	.float		modelindex;

	// physics description
	.vector		origin;
	.vector		angles;
	.vector		old_origin;
	.vector		old_angles;
	.vector		velocity;
	.vector		avelocity;
	.vector		post_origin;
	.vector		post_angles;
	.vector		post_velocity;
	.vector		post_avelocity;
	.vector		origin_offset;
	.vector		angles_offset;
	.float		ltime;

	.float		bouncetype;
	.float		movetype;
	.float		solid;
	.vector		absmin, absmax;
	.vector		mins, maxs;
	.vector		size;

	// entity base description
	.entity		chain;	// dynamic list of all ents
	.string		model;
	.float		frame;
	.float		sequence;
	.float		renderfx;
	.float		effects;
	.float		skin;
	.float		body;
	.string		weaponmodel;
	.float		weaponframe;
	
	// base generic funcs
	.void()		use;
	.void()		touch;
	.void()		think;
	.void()		blocked;
	.void()		activate;

	// npc generic funcs
	.void()		walk;
	.void()		jump;
	.void()		duck;

	// flags
	.float		flags;
	.float		aiflags;
	.float		spawnflags;
				
	// other variables
	.entity		groundentity;
	.float		nextthink;
	.float		takedamage;
	.float		health;

	.float		frags;
	.float		weapon;
	.float		items;
	.string		target;
	.string		parent;
	.string		targetname;
	.entity		aiment;		// attachment edict
	.entity		goalentity;
	.vector		punchangle;	
	.float		deadflag;
	.vector		view_ofs;		//.entity		viewheight;
	.float		button0;
	.float		button1;
	.float		button2;
	.float		impulse;
	.float		fixangle;
	.vector		v_angle;
	.float		idealpitch;
	.string		netname;
	.entity		enemy;
	.float		alpha;
	.float		team;
	.float		max_health;
	.float		teleport_time;
	.float		armortype;
	.float		armorvalue;
	.float		waterlevel;
	.float		watertype;
	.float		ideal_yaw;
	.float		yaw_speed;
	.float		dmg_take;
	.float		dmg_save;
	.entity		dmg_inflictor;
	.entity		owner;
	.vector		movedir;
	.string		message;
	.float		sounds;
	.string		noise;
	.string		noise1;
	.string		noise2;
	.string		noise3;		//a looped sound case
	.float		jumpup;
	.float		jumpdn;
	.entity		movetarget;
	.float		mass;
	.float		density;
	.float		gravity;
	.float		dmg;
	.float		dmgtime;
	.float		speed;
void		end_sys_fields;			// flag for structure dumping
// End. Lines below this MAY be altered, to some extent

#include "utils.h"

//
// constants
//

float	FALSE					= 0;
float 	TRUE					= 1;

// newdefines
#define	CS_NAME			0
#define	CS_SKY			1
#define	CS_SKYAXIS		2 // %f %f %f format
#define	CS_SKYROTATE		3
#define	CS_STATUSBAR		4 // hud_program section name
//NOTE: other  CS_* will be set by engine

#define	STAT_HEALTH_ICON		0
#define	STAT_HEALTH		1
#define	STAT_AMMO_ICON		2
#define	STAT_AMMO			3
#define	STAT_ARMOR_ICON		4
#define	STAT_ARMOR		5
#define	STAT_SELECTED_ICON		6
#define	STAT_PICKUP_ICON		7
#define	STAT_PICKUP_STRING		8
#define	STAT_TIMER_ICON		9
#define	STAT_TIMER		10
#define	STAT_HELPICON		11
#define	STAT_SELECTED_ITEM		12
#define	STAT_LAYOUTS		13
#define	STAT_FRAGS		14
#define	STAT_FLASHES		15		// cleared each frame, 1 = health, 2 = armor
#define	STAT_CHASE		16
#define	STAT_SPECTATOR		17
#define	STAT_SPEED		22
#define	STAT_ZOOM			23
#define	MAX_STATS			32

// edict.aiflags
#define AI_FLY			1	// monster is flying
#define AI_SWIM			2	// swimming monster
#define AI_ONGROUND			4	// monster is onground
#define AI_PARTIALONGROUND		8	// monster is partially onground
#define AI_GODMODE			16	// monster don't give damage at all
#define AI_NOTARGET			32	// monster will no searching enemy's
#define AI_NOSTEP			64	// Lazarus stuff
#define AI_DUCKED			128	// monster (or player) is ducked
#define AI_JUMPING			256	// monster (or player) is jumping
#define AI_FROZEN			512	// stop moving, but continue thinking
#define AI_ACTOR                	1024	// disable ai for actor
#define AI_DRIVER			2048	// npc or player driving vehcicle or train
#define AI_SPECTATOR		4096	// spectator mode for clients

// edict.flags
#define	FL_CLIENT			1	// this is client
#define	FL_MONSTER		2	// this is npc
#define	FL_DEADMONSTER		4
#define	FL_WORLDBRUSH		8	// Not moveable/removeable brush entity
#define	FL_DORMANT		16	// Entity is dormant, no updates to client
#define	FL_FRAMETHINK		32	// entity will be thinking every frame
#define	FL_GRAPHED		64	// ainode list member 
#define	FL_FLOAT			128	// this entity can be floating. FIXME: remove this ?
#define	FL_TRACKTRAIN		256	// this is tracktrain entity

// edict.movetype values
enum
{
	MOVETYPE_NONE,	// never moves
	MOVETYPE_NOCLIP,	// origin and angles change with no interaction
	MOVETYPE_PUSH,	// no clip to world, push on box contact
	MOVETYPE_WALK,	// gravity
	MOVETYPE_STEP,	// gravity, special edge handling
	MOVETYPE_FLY,
	MOVETYPE_TOSS,	// gravity
	MOVETYPE_BOUNCE,
	MOVETYPE_FOLLOW,	// attached models
	MOVETYPE_CONVEYOR,
	MOVETYPE_PUSHABLE,
	MOVETYPE_PHYSIC	// phys simulation
};

#define	RF_MINLIGHT		1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		4		// only draw through eyes
#define	RF_FULLBRIGHT		8		// allways draw full intensity
#define	RF_DEPTHHACK		16		// for view weapon Z crunching
#define	RF_TRANSLUCENT		32
#define	RF_FRAMELERP		64
#define	RF_BEAM			128
#define	RF_CUSTOMSKIN		256		// skin is an index in image_precache
#define	RF_GLOW			512		// pulse lighting for bonus items

// edict.solid values
enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_BOX,	// physbox
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH	// custom convex hull
};

// range values
float	RANGE_MELEE				= 0;
float	RANGE_NEAR				= 1;
float	RANGE_MID				= 2;
float	RANGE_FAR				= 3;

// deadflag values

float	DEAD_NO					= 0;
float	DEAD_DYING				= 1;
float	DEAD_DEAD				= 2;
float	DEAD_RESPAWNABLE		= 3;

// takedamage values

float	DAMAGE_NO				= 0;
float	DAMAGE_YES				= 1;
float	DAMAGE_AIM				= 2;

.void()		th_stand;
.void()		th_walk;
.void()		th_run;
.void(entity attacker, float damage)		th_pain;
.void()		th_die;
.void()         th_missile;
.void()         th_melee;

// point content values

float	CONTENT_EMPTY			= -1;
float	CONTENT_SOLID			= -2;
float	CONTENT_WATER			= -3;
float	CONTENT_SLIME			= -4;
float	CONTENT_LAVA			= -5;
float   CONTENT_SKY                     = -6;

float   STATE_RAISED            = 0;
float   STATE_LOWERED           = 1;
float	STATE_UP		= 2;
float	STATE_DOWN		= 3;

vector	VEC_ORIGIN = '0 0 0';
vector	VEC_HULL_MIN = '-16 -16 -24';
vector	VEC_HULL_MAX = '16 16 32';

vector	VEC_HULL2_MIN = '-32 -32 -24';
vector	VEC_HULL2_MAX = '32 32 64';

// protocol bytes
#define	SVC_TEMP_ENTITY		1
#define	SVC_LAYOUT		2
#define	SVC_INVENTORY		3

enum
{
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_FOOTSTEP,
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_PLAYER_TELEPORT,
	EV_OTHER_TELEPORT
};

float	TE_SPIKE		= 0;
float	TE_SUPERSPIKE	= 1;
float	TE_GUNSHOT		= 2;
float	TE_EXPLOSION	= 3;
float	TE_TAREXPLOSION	= 4;
float	TE_LIGHTNING1	= 5;
float	TE_LIGHTNING2	= 6;
float	TE_WIZSPIKE		= 7;
float	TE_KNIGHTSPIKE	= 8;
float	TE_LIGHTNING3	= 9;
float	TE_LAVASPLASH	= 10;
float	TE_TELEPORT		= 11;

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
float	CHAN_AUTO		= 0;
float	CHAN_WEAPON		= 1;
float	CHAN_VOICE		= 2;
float	CHAN_ITEM		= 3;
float	CHAN_BODY		= 4;

float	ATTN_NONE		= 0;
float	ATTN_NORM		= 1;
float	ATTN_IDLE		= 2;
float	ATTN_STATIC		= 3;

// update types

float	UPDATE_GENERAL	= 0;
float	UPDATE_STATIC	= 1;
float	UPDATE_BINARY	= 2;
float	UPDATE_TEMP		= 3;

// entity effects

float	EF_TELEPORT	= 1;
float	EF_ROTATE 	= 2;

float	AS_STRAIGHT		= 1;
float	AS_SLIDING		= 2;
float	AS_MELEE		= 3;
float	AS_MISSILE		= 4;

void() SUB_Null = {};
void() SUB_Null2 = {};
	
// Quake assumes these are defined.
entity activator;
string string_null;    // null string, nothing should be held here

.string         wad, map, landmark;
.float worldtype, delay, wait, lip, light_lev, speed, style, skill;
.string killtarget;
.vector pos1, pos2, mangle;

void() SUB_Remove = {remove(pev);};
// End

// Damage.qc
entity damage_attacker;
.float pain_finished, air_finished, dmg, dmgtime;

//ChaseCAm
.vector camview;
.float aflag;
.entity trigger_field;
