//=======================================================================
//			Copyright XashXT Group 2007 ©
//			const.h - shared engine constants
//=======================================================================
#ifndef CONST_H
#define CONST_H

// euler angle order
#define PITCH			0
#define YAW			1
#define ROLL			2

#define VOL_NORM			1.0	// volume values

// pitch values
#define PITCH_LOW			95	// other values are possible - 0-255, where 255 is very high
#define PITCH_NORM			100	// non-pitch shifted
#define PITCH_HIGH			120

// attenuation values
#define ATTN_NONE			0
#define ATTN_NORM			0.8f
#define ATTN_IDLE			2.0f
#define ATTN_STATIC			1.25f 

// channels
#define CHAN_AUTO			0
#define CHAN_WEAPON			1
#define CHAN_VOICE			2
#define CHAN_ITEM			3
#define CHAN_BODY			4
#define CHAN_STREAM			5	// allocate stream channel from the static or dynamic area
#define CHAN_STATIC			6	// allocate channel from the static area 

// in buttons
#define IN_ATTACK			(1<<0)
#define IN_JUMP			(1<<1)
#define IN_DUCK			(1<<2)
#define IN_FORWARD			(1<<3)
#define IN_BACK			(1<<4)
#define IN_USE			(1<<5)
#define IN_CANCEL			(1<<6)
#define IN_LEFT			(1<<7)
#define IN_RIGHT			(1<<8)
#define IN_MOVELEFT			(1<<9)
#define IN_MOVERIGHT		(1<<10)
#define IN_ATTACK2			(1<<11)
#define IN_RUN			(1<<12)
#define IN_RELOAD			(1<<13)
#define IN_ALT1			(1<<14)
#define IN_SCORE			(1<<15)   // Used by client.dll for when scoreboard is held down

// edict_t->spawnflags
#define SF_START_ON			0x1

// edict->flags
#define FL_CLIENT			(1<<0)
#define FL_MONSTER			(1<<1)	// monster bit
#define FL_INWATER			(1<<2)
#define FL_INTERMISSION		(1<<3)
#define FL_ONGROUND			(1<<2)	// at rest / on the ground
#define FL_SKYENTITY		(1<<5)	// it's a env_sky entity
#define FL_WATERJUMP		(1<<6)	// player jumping out of water
#define FL_FLOAT			(1<<7)	// Apply floating force to this entity when in water
#define FL_GRAPHED			(1<<8)	// worldgraph has this ent listed as something that blocks a connection
#define FL_TANK			(1<<9)	// this is func tank
#define FL_ROCKET			(1<<10)	// this is rocket entity
#define FL_POINTENTITY		(1<<11)	// this is point entity
#define FL_PROXY			(1<<12)	// This is a spectator proxy
#define FL_FRAMETHINK		(1<<13)	// Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY		(1<<14)	// Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP		(1<<15)	// Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN			(1<<16)	// Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH		(1<<17)	// Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR            	(1<<18)	// This client is a spectator, don't run touch functions, etc.
#define FL_CUSTOMENTITY		(1<<19)	// This is a custom entity
#define FL_KILLME			(1<<20)	// This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT			(1<<21)	// Entity is dormant, no updates to client
#define FL_PARTIALONGROUND		(1<<22)	// not corners are valid

// edict->aiflags
#define AI_FLY			(1<<0)	// changes the SV_Movestep() behavior to not need to be on ground
#define AI_SWIM			(1<<1)	// same as AI_FLY but stay in water
#define AI_WATERJUMP		(1<<2)	// -- reserved --

#define AI_GODMODE			(1<<4)	// invulnerability npc or client
#define AI_NOTARGET			(1<<5)	// mark any npc as neytral
#define AI_NOSTEP			(1<<6)	// -- reserved --
#define AI_DUCKED			(1<<7)	// monster (or player) is ducked
#define AI_JUMPING			(1<<8)	// monster (or player) is jumping
#define AI_FROZEN			(1<<9)	// stop moving, but continue thinking (e.g. for thirdperson camera)
#define AI_ACTOR                	(1<<10)	// npc that playing scriped_sequence
#define AI_DRIVER			(1<<11)	// npc or player driving vehcicle or train
#define AI_SPECTATOR		(1<<12)	// spectator mode for clients


// entity_state_t->effects
#define EF_BRIGHTFIELD		(1<<0)	// swirling cloud of particles
#define EF_MUZZLEFLASH		(1<<1)	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT		(1<<2)	// DLIGHT centered at entity origin
#define EF_DIMLIGHT			(1<<3)	// player flashlight
#define EF_INVLIGHT			(1<<4)	// get lighting from ceiling
#define EF_NOINTERP			(1<<5)	// don't interpolate the next frame
#define EF_NODRAW			(1<<6)	// don't draw entity
#define EF_ROTATE			(1<<7)	// rotate bonus item
#define EF_MINLIGHT			(1<<8)	// allways have some light (viewmodel)
#define EF_LIGHT			(1<<9)	// dynamic light (rockets use)

// edict->deadflag values
#define DEAD_NO		0	// alive
#define DEAD_DYING		1	// playing death animation or still falling off of a ledge waiting to hit ground
#define DEAD_DEAD		2	// dead. lying still.
#define DEAD_RESPAWNABLE	3	// wait for respawn
#define DEAD_DISCARDBODY	4

#define DAMAGE_NO		0
#define DAMAGE_YES		1
#define DAMAGE_AIM		2

// engine edict types
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
	ED_MISSILE,	// greande, rocket e.t.c
	ED_DECAL,		// render will be merge real coords and normal
	ED_VEHICLE,	// controllable vehicle
	ED_MAXTYPES,
} edtype_t;

// edict movetype
typedef enum
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
	MOVETYPE_PUSHSTEP,
	MOVETYPE_PHYSIC,	// phys simulation
} movetype_t;

// edict collision modes
typedef enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_SLIDEBOX,	//
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_BOX,	// physbox
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH,	// custom convex hull
} solid_t;

typedef enum
{
	point_hull = 0,
	human_hull = 1,
	large_hull = 2,
	head_hull = 3
};

// beam types, encoded as a byte
typedef enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
} kBeamType_t;

// lower bits encoded as kBeamType_t (max 8 types)
#define BEAM_FSINE		(1<<3)
#define BEAM_FSOLID		(1<<4)
#define BEAM_FSHADEIN	(1<<5)
#define BEAM_FSHADEOUT	(1<<6)

// rendering constants
typedef enum 
{	
	kRenderNormal,		// src
	kRenderTransColor,		// c*a+dest*(1-a)
	kRenderTransTexture,	// src*a+dest*(1-a)
	kRenderGlow,		// src*a+dest -- no Z buffer checks
	kRenderTransAlpha,		// src*srca+dest*(1-srca)
	kRenderTransAdd,		// src*a+dest
} kRenderMode_t;

typedef enum 
{	
	kRenderFxNone = 0, 

	// legacy stuff are not supported

	kRenderFxNoDissipation = 14,
	kRenderFxDistort,			// Distort/scale/translate flicker
	kRenderFxHologram,			// kRenderFxDistort + distance fade
	kRenderFxDeadPlayer,		// kRenderAmt is the player index
	kRenderFxExplode,			// Scale up really big!
	kRenderFxGlowShell,			// Glowing Shell
	kRenderFxClampMinScale,		// Keep this sprite from getting very small (SPRITES only!)
	kRenderFxAurora,			// set particle trail for this entity
	kRenderFxNoReflect,			// don't reflecting in mirrors 
} kRenderFx_t;

#endif//CONST_H