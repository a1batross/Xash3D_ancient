//=======================================================================
//			Copyright XashXT Group 2007 ©
//			  const.h - engine constants
//=======================================================================
#ifndef CONST_H
#define CONST_H

// euler angle order
#define PITCH			0
#define YAW			1
#define ROLL			2

// sound specific
#define VOL_NORM			1.0f	// volume values

// pitch values
#define PITCH_LOW			95	// other values are possible - 0-255, where 255 is very high
#define PITCH_NORM			100	// non-pitch shifted
#define PITCH_HIGH			120

// attenuation values
#define ATTN_NONE			0.0f
#define ATTN_NORM			0.8f
#define ATTN_IDLE			2.0f
#define ATTN_STATIC			1.25f 

// common conversion tools
#define ATTN_TO_SNDLVL( a )		(int)((a) ? (50 + 20 / ((float)a)) : 0 )
#define SNDLVL_TO_ATTN( a )		((a > 50) ? (20.0f / (float)(a - 50)) : 4.0 )

#define SND_CHANGE_VOL		(1<<0)	// change sound vol
#define SND_CHANGE_PITCH		(1<<1)	// change sound pitch
#define SND_STOP			(1<<2)	// stop the sound
#define SND_SPAWNING		(1<<3)	// we're spawing, used in some cases for ambients
#define SND_DELAY			(1<<4)	// sound has an initial delay
#define SND_STOP_LOOPING		(1<<5)	// stop all looping sounds on the entity.
#define SND_SPEAKER			(1<<6)	// being played again by a microphone through a speaker 

// 7 channels available
#define CHAN_REPLACE		-1	// force to replace sound for any channel
#define CHAN_AUTO			0
#define CHAN_WEAPON			1
#define CHAN_VOICE			2
#define CHAN_ITEM			3
#define CHAN_BODY			4
#define CHAN_STREAM			5	// allocate stream channel from the static or dynamic area
#define CHAN_STATIC			6	// allocate channel from the static area
#define CHAN_VOICE_BASE		7	// allocate channel for network voice data

#define MSG_BROADCAST		0	// unreliable to all
#define MSG_ONE			1	// reliable to one (msg_entity)
#define MSG_ALL			2	// reliable to all
#define MSG_INIT			3	// write to the init string
#define MSG_PVS			4	// Ents in PVS of org
#define MSG_PAS			5	// Ents in PAS of org
#define MSG_PVS_R			6	// Reliable to PVS
#define MSG_PAS_R			7	// Reliable to PAS
#define MSG_ONE_UNRELIABLE		8	// Send to one client, but don't put in reliable stream, put in unreliable datagram ( could be dropped )
#define MSG_SPEC			9	// Sends to all spectator proxies

#define CONTENTS_EMPTY		-1
#define CONTENTS_SOLID		-2
#define CONTENTS_WATER		-3
#define CONTENTS_SLIME		-4
#define CONTENTS_LAVA		-5
#define CONTENTS_SKY		-6
#define CONTENTS_ORIGIN		-7	// removed at csg time
#define CONTENTS_CLIP		-8	// changed to contents_solid
#define CONTENTS_CURRENT_0		-9
#define CONTENTS_CURRENT_90		-10
#define CONTENTS_CURRENT_180		-11
#define CONTENTS_CURRENT_270		-12
#define CONTENTS_CURRENT_UP		-13
#define CONTENTS_CURRENT_DOWN		-14
#define CONTENTS_TRANSLUCENT		-15
#define CONTENTS_LADDER		-16
#define CONTENTS_FLYFIELD		-17
#define CONTENTS_GRAVITY_FLYFIELD	-18
#define CONTENTS_FOG		-19

// global deatchmatch dmflags
#define DF_NO_HEALTH		(1<<0)
#define DF_NO_ITEMS			(1<<1)
#define DF_WEAPONS_STAY		(1<<2)
#define DF_NO_FALLING		(1<<3)
#define DF_INSTANT_ITEMS		(1<<4)
#define DF_SAME_LEVEL		(1<<5)
#define DF_SKINTEAMS		(1<<6)
#define DF_MODELTEAMS		(1<<7)
#define DF_NO_FRIENDLY_FIRE		(1<<8)
#define DF_SPAWN_FARTHEST		(1<<9)
#define DF_FORCE_RESPAWN		(1<<10)
#define DF_NO_ARMOR			(1<<11)
#define DF_ALLOW_EXIT		(1<<12)
#define DF_INFINITE_AMMO		(1<<13)
#define DF_QUAD_DROP		(1<<14)
#define DF_FIXED_FOV		(1<<15)
#define DF_QUADFIRE_DROP		(1<<16)
#define DF_NO_MINES			(1<<17)
#define DF_NO_STACK_DOUBLE		(1<<18)
#define DF_NO_NUKES			(1<<19)
#define DF_NO_SPHERES		(1<<20)

// common EDICT flags

// pev->flags
#define FL_FLY			(1<<0)	// changes the SV_Movestep() behavior to not need to be on ground
#define FL_SWIM			(1<<1)	// same as AI_FLY but stay in water
#define FL_CONVEYOR			(1<<2)	// not used in Xash3D
#define FL_CLIENT			(1<<3)	// this a client entity
#define FL_INWATER			(1<<4)	// npc in water
#define FL_MONSTER			(1<<5)	// monster bit
#define FL_GODMODE			(1<<6)	// invulnerability npc or client
#define FL_NOTARGET			(1<<7)	// mark all npc's as neytral
#define FL_SKIPLOCALHOST		(1<<8)	// Don't send entity to local host, it's predicting this entity itself
#define FL_ONGROUND			(1<<9)	// at rest / on the ground
#define FL_PARTIALGROUND		(1<<10)	// not corners are valid
#define FL_WATERJUMP		(1<<11)	// water jumping
#define FL_FROZEN			(1<<12)	// stop moving, but continue thinking (e.g. for thirdperson camera)
#define FL_FAKECLIENT		(1<<13)	// JAC: fake client, simulated server side; don't send network messages to them
#define FL_DUCKING			(1<<14)	// monster (or player) is ducked
#define FL_FLOAT			(1<<15)	// Apply floating force to this entity when in water
#define FL_GRAPHED			(1<<16)	// worldgraph has this ent listed as something that blocks a connection
#define FL_PROJECTILE		(1<<17)	// this is rocket entity    (was FL_IMMUNE_WATER)
#define FL_TANK			(1<<18)	// this is func tank entity (was FL_IMMUNE_SLIME)
#define FL_POINTENTITY		(1<<19)	// this is point entity     (was FL_IMMUNE_LAVA)
#define FL_PROXY			(1<<20)   // This is a spectator proxy
#define FL_ALWAYSTHINK		(1<<21)	// Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY		(1<<22)	// Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP		(1<<23)	// Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN			(1<<24)	// Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH		(1<<25)	// Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR            	(1<<26)	// This client is a spectator, don't run touch functions, etc.
#define FL_PHS_FILTER		(1<<27)	// This entity requested phs bitvector in AddToFullPack calls
#define FL_CUSTOMENTITY		(1<<29)	// This is a custom entity
#define FL_KILLME			(1<<30)	// This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT			(1<<31)	// Entity is dormant, no updates to client

// pev->spawnflags
#define SF_START_ON			(1<<0)

// classic quake flags (must be not collide with any dll spawnflags - engine uses this)
#define SF_NOT_EASY			(1<<8)
#define SF_NOT_MEDIUM		(1<<9)
#define SF_NOT_HARD			(1<<10)
#define SF_NOT_DEATHMATCH		(1<<11)

// pev->effects
#define EF_BRIGHTFIELD		(1<<0)	// swirling cloud of particles
#define EF_MUZZLEFLASH		(1<<1)	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT		(1<<2)	// DLIGHT centered at entity origin
#define EF_DIMLIGHT			(1<<3)	// player flashlight
#define EF_INVLIGHT			(1<<4)	// get lighting from ceiling
#define EF_NOINTERP			(1<<5)	// don't interpolate the next frame
#define EF_LIGHT			(1<<6)	// dynamic light (rockets use)
#define EF_NODRAW			(1<<7)	// don't draw entity
#define EF_ROTATE			(1<<8)	// rotate bonus item
#define EF_MINLIGHT			(1<<9)	// allways have some light (viewmodel)
#define EF_FULLBRIGHT		(1<<10)	// completely ignore light values
#define EF_ANIMATE			(1<<11)	// do client animate (ignore v.frame)
#define EF_NOSHADOW			(1<<12)	// ignore shadow for this entity
#define EF_PLANARSHADOW		(1<<13)	// use fast planarshadow method instead of shadow casters
#define EF_OCCLUSIONTEST		(1<<14)	// use occlusion test for this entity (e.g. glares)

// pev->takedamage
#define DAMAGE_NO			0	// can't be damaged
#define DAMAGE_YES			1	// attempt to damage
#define DAMAGE_AIM			2	// special case for aiming damage

// pev->deadflag
#define DEAD_NO			0	// alive
#define DEAD_DYING			1	// playing death animation or still falling off of a ledge waiting to hit ground
#define DEAD_DEAD			2	// dead. lying still.
#define DEAD_RESPAWNABLE		3	// wait for respawn
#define DEAD_DISCARDBODY		4

#define PM_MAXHULLS			4     	// 4 hulls - quake1, half-life

// filter console messages
typedef enum
{
	at_console = 1,	// format: [msg]
	at_warning,	// format: Warning: [msg]
	at_error,		// format: Error: [msg]
	at_loading,	// print messages during loading
	at_aiconsole,	// same as at_console, but only shown if developer level is 5!
	at_logged		// server print to console ( only in multiplayer games ). (NOT IMPLEMENTED)
} ALERT_TYPE;

// filter client messages
typedef enum
{
	print_console,	// dev. console messages
	print_center,	// at center of the screen
	print_chat,	// level high
} PRINT_TYPE;

// monster's walkmove modes
typedef enum
{
	WALKMOVE_NORMAL = 0,// normal walkmove
	WALKMOVE_WORLDONLY,	// doesn't hit ANY entities, no matter what the solid type
	WALKMOVE_CHECKONLY	// move, but don't touch triggers
} walkmove_t;

// monster's move to origin stuff
#define MOVE_START_TURN_DIST		64	// when this far away from moveGoal, start turning to face next goal
#define MOVE_STUCK_DIST		32	// if a monster can't step this far, it is stuck.
#define MOVE_NORMAL			0	// normal move in the direction monster is facing
#define MOVE_STRAFE			1	// moves in direction specified, no matter which way monster is facing
	
// edict movetype
typedef enum
{
	MOVETYPE_NONE = 0,	// never moves
	MOVETYPE_CONVEYOR,	// simulate conveyor belt, push all stuff
	MOVETYPE_STOP,	// toggled between PUSHSTEP and STOP
	MOVETYPE_WALK,	// Player only - moving on the ground
	MOVETYPE_STEP,	// gravity, special edge handling
	MOVETYPE_FLY,       // No gravity, but still collides with stuff
	MOVETYPE_TOSS,	// gravity/collisions
	MOVETYPE_PUSH,	// no clip to world, push on box contact
	MOVETYPE_NOCLIP,	// origin and angles change with no interaction
	MOVETYPE_FLYMISSILE,// extra size to monsters
	MOVETYPE_BOUNCE,	// Just like Toss, but reflect velocity when contacting surfaces
	MOVETYPE_BOUNCEMISSILE,// bounce w/o gravity
	MOVETYPE_FOLLOW,	// attached models
	MOVETYPE_PUSHSTEP,  // BSP model that needs physics/world collisions
	MOVETYPE_PHYSIC,	// phys simulation
} movetype_t;

// edict collision filter
typedef enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_SLIDEBOX,	// touch on edge, but not an onground
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_MESH,	// custom convex mesh
} solid_t;

// pev->buttons (client only)
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

// beam types, encoded as a byte
typedef enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
} kBeamType_t;

// lower bits encoded as kBeamType_t (max 8 types)
#define BEAM_FSINE			(1<<3)
#define BEAM_FSOLID			(1<<4)
#define BEAM_FSHADEIN		(1<<5)
#define BEAM_FSHADEOUT		(1<<6)

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

// studio models event range
#define EVENT_SPECIFIC		0
#define EVENT_SCRIPTED		1000
#define EVENT_SHARED		2000	// both client and server valid for playing
#define EVENT_CLIENT		5000	// less than this value it's a server-side studio events

// game print flags
#define PRINT_LOW			0	// pickup messages
#define PRINT_MEDIUM		1	// death messages
#define PRINT_HIGH			2	// critical messages
#define PRINT_CHAT			3	// chat messages

// client screen state
#define CL_LOADING			1	// draw loading progress-bar
#define CL_ACTIVE			2	// draw normal hud
#define CL_PAUSED			3	// pause when active

// built-in particle-system flags
#define PARTICLE_GRAVITY		40	// default particle gravity

#define PARTICLE_BOUNCE		(1<<0)	// makes a bouncy particle
#define PARTICLE_FRICTION		(1<<1)
#define PARTICLE_VERTEXLIGHT		(1<<2)	// give some ambient light for it
#define PARTICLE_STRETCH		(1<<3)
#define PARTICLE_UNDERWATER		(1<<4)
#define PARTICLE_INSTANT		(1<<5)

// built-in decals flags
#define DECAL_FADEALPHA		(1<<0)	// fade decal by alpha instead color
#define DECAL_FADEENERGY		(1<<1)	// fade decal energy balls

// built-in dlight flags
#define DLIGHT_FADE			(1<<0)	// fade dlight at end of lifetime

// renderer flags
#define RDF_NOWORLDMODEL		(1<<0) 	// used for player configuration screen
#define RDF_OLDAREABITS		(1<<1) 	// forces R_MarkLeaves if not set
#define RDF_PORTALINVIEW		(1<<2)	// cull entities using vis too because pvs\areabits are merged serverside
#define RDF_SKYPORTALINVIEW		(1<<3)	// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		(1<<4)	// do not adjust fov for widescreen
#define RDF_THIRDPERSON		(1<<5)	// enable chase cam instead firstperson

// client modelindexes
#define NULLENT_INDEX		-1	// engine always return NULL, only for internal use
#define VIEWENT_INDEX		-2	// can get viewmodel for local client

#endif//CONST_H