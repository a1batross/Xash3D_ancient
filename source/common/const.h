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
#define ATTN_RICOCHET		1.5f
#define ATTN_GUNFIRE		0.27f

typedef enum
{
	SNDLVL_NONE	= 0,
	SNDLVL_25dB	= 25,
	SNDLVL_30dB	= 30,
	SNDLVL_35dB	= 35,
	SNDLVL_40dB	= 40,
	SNDLVL_45dB	= 45,
	SNDLVL_50dB	= 50,	// 3.9
	SNDLVL_55dB	= 55,	// 3.0
	SNDLVL_IDLE	= 60,	// 2.0
	SNDLVL_60dB	= 60,	// 2.0
	SNDLVL_65dB	= 65,	// 1.5
	SNDLVL_STATIC	= 66,	// 1.25
	SNDLVL_70dB	= 70,	// 1.0
	SNDLVL_NORM	= 75,
	SNDLVL_75dB	= 75,	// 0.8
	SNDLVL_80dB	= 80,	// 0.7
	SNDLVL_TALKING	= 80,	// 0.7
	SNDLVL_85dB	= 85,	// 0.6
	SNDLVL_90dB	= 90,	// 0.5
	SNDLVL_95dB	= 95,
	SNDLVL_100dB	= 100,	// 0.4
	SNDLVL_105dB	= 105,
	SNDLVL_110dB	= 110,
	SNDLVL_120dB	= 120,
	SNDLVL_130dB	= 130,
	SNDLVL_GUNFIRE	= 140,	// 0.27
	SNDLVL_140dB	= 140,	// 0.2
	SNDLVL_150dB	= 150,	// 0.2
} soundlevel_t;

// common conversion tools
#define ATTN_TO_SNDLVL( a )		(soundlevel_t)(int)((a) ? (50 + 20 / ((float)a)) : 0 )
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
#define FL_CLIENT			(1<<2)	// this a client entity
#define FL_INWATER			(1<<3)	// npc in water
#define FL_MONSTER			(1<<4)	// monster bit
#define FL_GODMODE			(1<<5)	// invulnerability npc or client
#define FL_NOTARGET			(1<<6)	// mark all npc's as neytral
#define FL_ONGROUND			(1<<7)	// at rest / on the ground
#define FL_PARTIALONGROUND		(1<<8)	// not corners are valid
#define FL_WATERJUMP		(1<<8)	// water jumping
#define FL_FROZEN			(1<<9)	// stop moving, but continue thinking (e.g. for thirdperson camera)
#define FL_DUCKING			(1<<10)	// monster (or player) is ducked
#define FL_FLOAT			(1<<11)	// Apply floating force to this entity when in water
#define FL_GRAPHED			(1<<12)	// worldgraph has this ent listed as something that blocks a connection
#define FL_ALWAYSTHINK		(1<<13)	// Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_PROJECTILE		(1<<14)	// this is rocket entity
#define FL_TANK			(1<<15)	// this is func tank entity
#define FL_ONTRAIN			(1<<16)	// Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH		(1<<17)	// Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR            	(1<<18)	// This client is a spectator, don't run touch functions, etc.
#define FL_CUSTOMENTITY		(1<<19)	// This is a custom entity
#define FL_KILLME			(1<<20)	// This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT			(1<<21)	// Entity is dormant, no updates to client
#define FL_POINTENTITY		(1<<22)	// this is point entity
#define FL_FAKECLIENT		(1<<23)	// JAC: fake client, simulated server side; don't send network messages to them

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
#define EF_NODRAW			(1<<6)	// don't draw entity
#define EF_ROTATE			(1<<7)	// rotate bonus item
#define EF_MINLIGHT			(1<<8)	// allways have some light (viewmodel)
#define EF_FULLBRIGHT		(1<<9)	// completely ignore light values
#define EF_LIGHT			(1<<10)	// dynamic light (rockets use)
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

// edict collision filter
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

// client key destination
#define KEY_GAME			1
#define KEY_HUDMENU			2

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

// engine built-in default shader
#define MAP_DEFAULT_SHADER		"*black"

// client modelindexes
#define NULLENT_INDEX		-1	// engine always return NULL, only for internal use
#define VIEWENT_INDEX		-2	// can get viewmodel for local client

// basic console charwidths
#define TINYCHAR_WIDTH		(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT		(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT		16
#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		24
#define GIANTCHAR_WIDTH		32
#define GIANTCHAR_HEIGHT		48

#endif//CONST_H