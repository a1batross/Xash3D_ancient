//=======================================================================
//			Copyright XashXT Group 2007 ©
//			const.h - shared engine constants
//=======================================================================
#ifndef CONST_H
#define CONST_H

// shared typedefs
typedef unsigned __int64		qword;
typedef unsigned long		dword;
typedef unsigned int		uint;
typedef unsigned short		word;
typedef unsigned char		byte;
typedef int			shader_t;

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
#define FL_FLY			(1<<0)	// changes the SV_Movestep() behavior to not need to be on ground
#define FL_SWIM			(1<<1)	// same as AI_FLY but stay in water
#define FL_CLIENT			(1<<2)
#define FL_INWATER			(1<<3)
#define FL_MONSTER			(1<<4)	// monster bit
#define FL_GODMODE			(1<<5)	// invulnerability npc or client
#define FL_NOTARGET			(1<<6)	// mark any npc as neytral
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

typedef enum
{
	at_console = 1,	// format: [msg]
	at_warning,	// format: Warning: [msg]
	at_error,		// format: Error: [msg]
	at_loading,	// print messages during loading
	at_aiconsole,	// same as at_console, but only shown if developer level is 5!
	at_logged		// server print to console ( only in multiplayer games ).
} ALERT_TYPE;

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

// player_state_t->renderfx
#define RDF_UNDERWATER		(1<<0)	// warp the screen as apropriate
#define RDF_NOWORLDMODEL		(1<<1)	// used for player configuration screen
#define RDF_BLOOM			(1<<2)	// light blooms

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH			640
#define SCREEN_HEIGHT			480

// client screen state
#define CL_DISCONNECTED		1	//
#define CL_LOADING			2	// draw loading progress-bar
#define CL_ACTIVE			3	// draw normal hud

#define TINYCHAR_WIDTH		(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT		(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT		16
#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		24
#define GIANTCHAR_WIDTH		32
#define GIANTCHAR_HEIGHT		48

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE		2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

#define INTERFACE_VERSION		1	// both the client and server iface version

//=======================================================================
//
//		server.dll - client.dll definitions only
//
//=======================================================================
#define MAX_WEAPONS			32
#define MAX_AMMO_SLOTS  		32

#define HIDEHUD_WEAPONS		BIT( 0 )
#define HIDEHUD_FLASHLIGHT		BIT( 1 )
#define HIDEHUD_ALL			BIT( 2 )
#define HIDEHUD_HEALTH		BIT( 3 )
#define ITEM_SUIT			BIT( 4 )

enum ShakeCommand_t
{
	SHAKE_START = 0,	// Starts the screen shake for all players within the radius.
	SHAKE_STOP,	// Stops the screen shake for all players within the radius.
	SHAKE_AMPLITUDE,	// Modifies the amplitude of an active screen shake for all players within the radius.
	SHAKE_FREQUENCY,	// Modifies the frequency of an active screen shake for all players within the radius.
};

#define FFADE_IN		0x0000 // Just here so we don't pass 0 into the function
#define FFADE_OUT		0x0001 // Fade out (not in)
#define FFADE_MODULATE	0x0002 // Modulate (don't blend)
#define FFADE_STAYOUT	0x0004 // ignores the duration, stays faded out until new ScreenFade message received
#define FFADE_CUSTOMVIEW	0x0008 // fading only at custom viewing (don't sending this to engine )

#endif//CONST_H