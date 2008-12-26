//=======================================================================
//			Copyright (C) Shambler Team 2004
//		defaults.h - default global settings for max ammo, 
//			    monster's health, e.t.c
//=======================================================================

#ifndef DEFAULT_H
#define DEFAULT_H

//name of directory with mod
#define PITCHMIN				30  //min pitch	
#define PITCHMAX				100 //max pitch

#define MAX_AVELOCITY			4096
#define MAX_VELOCITY			4096
#define MAX_TRANSITION_ENTITY			512
#define MAP_MAX_NAME			32

#define MAX_ITEM_TYPES			6 //selection slots
#define MAX_ITEMS				5 //item types

#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

//=========================
// 	Debug macros
//=========================
#define SHIFT				Msg("\n")
#define ACTION				Msg("Action!\n")
#define DEBUGHEAD				Msg("======/Xash Debug System/======\nclassname: %s, targetname %s\n", STRING(pev->classname), STRING(pev->targetname))
#define MSGSTATEHEALTH			Msg("State: %s, health %g\n", GetStringForState( GetState()), pev->health )			
#define MSGSTATESTRENGTH			Msg("State: %s, strength %g\n", GetStringForState( GetState()), pev->health )
#define MSGSTATEVOLUME			Msg("State: %s, volume %g\n", GetStringForState( GetState()), m_flVolume )
#define MSGSTATESPEED			Msg("State: %s, speed %g\n", GetStringForState( GetState()), pev->speed )

//=========================
// 	CLOCK DEFAULTS
//=========================
#define SECONDS				60
#define MINUTES				3600
#define HOURS				43200

//=========================
// 	LIGHT DEFAULTS
//=========================
#define START_SWITCH_STYLE			32

//=========================
// 	WORLD DEFAULTS
//=========================
#define XEN_GRAVITY				0.6
#define EARTH_GRAVITY			1.0
#define MAP_SIZE				8192
#define MAP_HALFSIZE			MAP_SIZE / 2
#define FOG_LIMIT				30000
#define MAX_GIB_LIFETIME			30

//=========================
// 	Global spawnflag system
//=========================
#define SF_START_ON				0x1
#define SF_NOTSOLID				0x2
#define SF_FIREONCE				0x2
#define SF_NORESPAWN			( 1 << 30 )
//=========================
// 	FCAP DEFAULTS
//=========================
// These are caps bits to indicate what an object's capabilities (currently used for save/restore and level transitions)
#define FCAP_CUSTOMSAVE		0x00000001
#define FCAP_ACROSS_TRANSITION	0x00000002		// should transfer between transitions
#define FCAP_MUST_SPAWN		0x00000004		// Spawn after restore
#define FCAP_IMPULSE_USE		0x00000008		// can be used by the player
#define FCAP_CONTINUOUS_USE		0x00000010		// can be used by the player
#define FCAP_ONOFF_USE		0x00000020		// can be used by the player
#define FCAP_DIRECTIONAL_USE		0x00000040		// Player sends +/- 1 when using (currently only tracktrains)
#define FCAP_FORCE_TRANSITION		0x00000080		// ALWAYS goes across transitions
#define FCAP_ONLYDIRECT_USE		0x00000100		//LRC - can't use this entity through a wall.
#define FCAP_DONT_SAVE		0x80000000		// Don't save this

//=========================
// 	MONSTER CLASSIFY
//=========================
// For CLASSIFY
#define CLASS_NONE			0
#define CLASS_MACHINE		1
#define CLASS_PLAYER 		2
#define CLASS_HUMAN_PASSIVE		3
#define CLASS_HUMAN_MILITARY		4
#define CLASS_ALIEN_MILITARY		5
#define CLASS_ALIEN_PASSIVE		6
#define CLASS_ALIEN_MONSTER		7
#define CLASS_ALIEN_PREY		8
#define CLASS_ALIEN_PREDATOR		9
#define CLASS_INSECT		10
#define CLASS_PLAYER_ALLY		11
#define CLASS_PLAYER_BIOWEAPON	12 // hornets and snarks.launched by players
#define CLASS_ALIEN_BIOWEAPON		13 // hornets and snarks.launched by the alien menace
#define CLASS_FACTION_A		14 // very simple new classes, for use with Behaves As
#define CLASS_FACTION_B		15
#define CLASS_FACTION_C		16
#define CLASS_BARNACLE		99 //special because no one pays attention to it, and it eats a wide cross-section of creatures.

//=========================
// 	DAMAGE DEFAULTS
//=========================
#define DMG_GENERIC			0	// generic damage was done
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object
#define DMG_BULLET			(1 << 1)	// shot
#define DMG_SLASH			(1 << 2)	// cut, clawed, stabbed
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_FREEZE			(1 << 4)	// frozen
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_BLAST			(1 << 6)	// explosive blast damage
#define DMG_CLUB			(1 << 7)	// crowbar, punch, headbutt
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_SONIC			(1 << 9)	// sound pulse shockwave
#define DMG_ENERGYBEAM		(1 << 10)	// laser or other high energy beam
#define DMG_NEVERGIB		(1 << 12)	// with this bit OR'd in, no damage type will be able to gib victims upon death
#define DMG_ALWAYSGIB		(1 << 13)	// with this bit OR'd in, any damage type can be made to gib victims upon death.
#define DMG_DROWN			(1 << 14)	// Drowning
#define DMG_TIMEBASED		(~(0x3fff))// mask for time-based damage
#define DMG_PARALYZE		(1 << 15)	// slows affected creature down
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_POISON			(1 << 17)	// blood poisioning
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER		(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven
#define DMG_SLOWFREEZE		(1 << 22)	// in a subzero freezer
#define DMG_MORTAR			(1 << 23)	// Hit by air raid (done to distinguish grenade from mortar)
#define DMG_NUCLEAR			(1 << 24) // dmg at nuclear explode

// these are the damage types that are allowed to gib corpses
#define DMG_GIB_CORPSE		( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB | DMG_NUCLEAR )
// these are the damage types that have client hud art
#define DMG_SHOWNHUD		(DMG_POISON | DMG_ACID | DMG_FREEZE | DMG_SLOWFREEZE | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK | DMG_NUCLEAR)

#define PARALYZE_DURATION		2		// number of 2 second intervals to take damage
#define PARALYZE_DAMAGE		1.0		// damage to take each 2 second interval
#define NERVEGAS_DURATION		2
#define NERVEGAS_DAMAGE		5.0
#define POISON_DURATION		5
#define POISON_DAMAGE		2.0
#define RADIATION_DURATION		2
#define RADIATION_DAMAGE		1.0
#define ACID_DURATION		2
#define ACID_DAMAGE			5.0
#define SLOWBURN_DURATION		2
#define SLOWBURN_DAMAGE		1.0
#define SLOWFREEZE_DURATION		2
#define SLOWFREEZE_DAMAGE		1.0

#define	itbd_Paralyze		0
#define	itbd_NerveGas		1
#define	itbd_Poison		2
#define	itbd_Radiation		3
#define	itbd_DrownRecover		4
#define	itbd_Acid			5
#define	itbd_SlowBurn		6
#define	itbd_SlowFreeze		7
#define	CDMG_TIMEBASED		8

// when calling KILLED(), a value that governs gib behavior is expected to be
// one of these three values
#define GIB_NORMAL			0// gib if entity was overkilled
#define GIB_NEVER			1// never gib, no matter how much death damage is done ( freezing, etc )
#define GIB_ALWAYS			2// always gib ( Houndeye Shock, Barnacle Bite )

//=========================
// 	CAPS DEFAULTS
//=========================
#define bits_CAP_DUCK		( 1 << 0 )// crouch
#define bits_CAP_JUMP		( 1 << 1 )// jump/leap
#define bits_CAP_STRAFE		( 1 << 2 )// strafe ( walk/run sideways)
#define bits_CAP_SQUAD		( 1 << 3 )// can form squads
#define bits_CAP_SWIM		( 1 << 4 )// proficiently navigate in water
#define bits_CAP_CLIMB		( 1 << 5 )// climb ladders/ropes
#define bits_CAP_USE		( 1 << 6 )// open doors/push buttons/pull levers
#define bits_CAP_HEAR		( 1 << 7 )// can hear forced sounds
#define bits_CAP_AUTO_DOORS		( 1 << 8 )// can trigger auto doors
#define bits_CAP_OPEN_DOORS		( 1 << 9 )// can open manual doors
#define bits_CAP_TURN_HEAD		( 1 << 10)// can turn head, always bone controller 0

#define bits_CAP_RANGE_ATTACK1	( 1 << 11)// can do a range attack 1
#define bits_CAP_RANGE_ATTACK2	( 1 << 12)// can do a range attack 2
#define bits_CAP_MELEE_ATTACK1	( 1 << 13)// can do a melee attack 1
#define bits_CAP_MELEE_ATTACK2	( 1 << 14)// can do a melee attack 2
#define bits_CAP_FLY		( 1 << 15)// can fly, move all around
#define bits_CAP_DOORS_GROUP		(bits_CAP_USE | bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS)


//=========================
// 	WEAPON DEFAULTS
//=========================
//sound volume for different states
#define PRIMARY_CHARGE_VOLUME		256
#define PRIMARY_FIRE_VOLUME		450

#define LOUD_GUN_VOLUME		1000
#define NORMAL_GUN_VOLUME		600
#define QUIET_GUN_VOLUME		200

//flash brightness
#define	BRIGHT_GUN_FLASH		512
#define 	NORMAL_GUN_FLASH		256
#define	DIM_GUN_FLASH		128
#define	NO_GUN_FLASH		0

#define WEAPON_NOCLIP			-1

//default as barney hands for weapon
#define GORDON_HANDS			1
#define BARNEY_HANDS			0
//Bullet damage settings
//#define 9MM_BULLET_DMG		8

//=========================
// SETTINGS FOR EACH WEAPON
//=========================

//Crowbar settings (weapon_crowbar)
#define CROWBAR_WEIGHT			0
#define CROWBAR_DMG		         		10
#define CROWBAR_BODYHIT_VOLUME		128
#define CROWBAR_WALLHIT_VOLUME		512

//Glock settings (weapon_glock)
#define GLOCK_WEIGHT			10
#define GLOCK_MAX_CARRY			250
#define GLOCK_MAX_CLIP			17
#define GLOCK_DEFAULT_GIVE			17
#define GLOCK_RANDOM_GIVE			RANDOM_LONG( 10, 17 )

//Desert eagle settings (weapon_eagle)
#define DESERT_MAX_CLIP			7
#define DESERT_WEIGHT			15
#define DESERT_DEFAULT_GIVE			7
#define DESERT_RANDOM_GIVE			RANDOM_LONG( 4, 7 )
#define DESERT_MAX_CARRY			21
#define DESERT_LASER_FOCUS			850

//mp5 settings (weapon_mp5)
#define MP5_WEIGHT				15
#define MP5_MAX_CLIP			50
#define MP5_DEFAULT_GIVE			25
#define MP5_RANDOM_GIVE			RANDOM_LONG( 19, 45 )
#define MP5_M203_DEFAULT_GIVE			0
#define MP5_M203_RANDOM_GIVE			RANDOM_LONG( 0, 3 )

//saw settings (weapon_m249)
#define SAW_WEIGHT				25
#define SAW_MAX_CARRY			200
#define SAW_MAX_CLIP			100
#define SAW_DEFAULT_GIVE			100
#define SAW_RANDOM_GIVE			RANDOM_LONG( 20, 100 )

//shotgun settings (weapon_shotgun)
#define SHOTGUN_WEIGHT			15
#define SHOTGUN_MAX_CLIP			6
#define SHOTGUN_DEFAULT_GIVE			6
#define SHOTGUN_RANDOM_GIVE			RANDOM_LONG( 1, 6 )
#define VECTOR_CONE_DM_SHOTGUN		Vector( 0.08716, 0.04362, 0.00  )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN 		Vector( 0.17365, 0.04362, 0.00 ) // 20 degrees by 5 degrees

//m40a1 settings (weapon_m40a1)
#define M40A1_WEIGHT			25
#define M40A1_MAX_CARRY			10
#define M40A1_MAX_CLIP			5
#define M40A1_DEFAULT_GIVE			5
#define M40A1_RANDOM_GIVE			RANDOM_LONG( 1, 5 )
#define MAX_ZOOM				15

//Rpg settings (weapon_rpg)
#define RPG_WEIGHT				20
#define RPG_MAX_CLIP			1
#define RPG_DEFAULT_GIVE			1
#define AMMO_RPGCLIP_GIVE			1
#define RPG_ROCKET_DMG			100
#define RPG_LASER_FOCUS			350

//gauss settings (weapon_gauss)
#define GAUSS_PRIMARY_DMG			20
#define GAUSS_WEIGHT			20
#define GAUSS_DEFAULT_GIVE			20
#define GAUSS_RANDOM_GIVE			RANDOM_LONG( 10, 40 )

//Egon settings (weapon_egon)
#define EGON_WEIGHT				20
#define EGON_DEFAULT_GIVE			20
#define EGON_RANDOM_GIVE			RANDOM_LONG( 15, 35 )
#define EGON_NARROW_DMG			6
#define EGON_WIDE_DMG			14
#define EGON_BEAM_SPRITE			"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE			"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP			"weapons/egon_windup2.wav"
#define EGON_SWITCH_NARROW_TIME		0.75
#define EGON_SWITCH_WIDE_TIME			1.5
#define EGON_PULSE_INTERVAL			0.1
#define EGON_DISCHARGE_INTERVAL		0.1

//Displacer settings (weapon_displacer)
#define DISPLACER_WEIGHT			10
#define DISPLACER_DEFAULT_GIVE		80
#define DISPLACER_RANDOM_GIVE			RANDOM_LONG( 40, 100 )
#define DBALL_DMG				100
#define DBALL_RADIUS			400

//hornetgun settings (weapon_hornetgun)
#define HORNETGUN_WEIGHT			10
#define HIVEHAND_DEFAULT_GIVE			8

//Handgrenade settings (weapon_handgrenade)
#define HANDGRENADE_WEIGHT			5
#define HANDGRENADE_DMG			100
#define HANDGRENADE_DEFAULT_GIVE		1

//Redeemder settings (weapon_redeemer)
#define REDEEMER_ROCKET_DMG			500
#define WARHEAD_SPEED			600
#define WARHEAD_SPEED_UNDERWATER		300
#define WARHEAD_MAX_SPEED			1400

//=========================
// 	AMMO SETTINGS
//=========================

//max capacity
#define	_9MM_MAX_CARRY			250
#define URANIUM_MAX_CARRY			100
#define BUCKSHOT_MAX_CARRY			125
#define BOLT_MAX_CARRY			50
#define ROCKET_MAX_CARRY			3
#define HANDGRENADE_MAX_CARRY			10
#define HORNET_MAX_CARRY			8
#define M203_GRENADE_MAX_CARRY		10

//ammo default give
#define AMMO_URANIUMBOX_GIVE		20
#define AMMO_GLOCKCLIP_GIVE		GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE		6
#define AMMO_MP5CLIP_GIVE		MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE		12
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_URANIUMBOX_GIVE		20

//=========================
// 	TIME TO RESPAWN
//=========================

#define RESPAWN_TIME_30SEC		30
#define RESPAWN_TIME_60SEC              60
#define RESPAWN_TIME_120SEC             120
#define RND_RESPAWN_TIME_30             RANDOM_FLOAT( 15, 45 )
#define RND_RESPAWN_TIME_60             RANDOM_FLOAT( 45, 80 )
#define RND_RESPAWN_TIME_120            RANDOM_FLOAT( 80, 145 )

//=========================
// BATTERY AND HEALTHKIT CAPACITY
//=========================

#define BATTERY_CHARGE		15
#define MAX_NORMAL_BATTERY		100

//=========================
// 	MONSTERS HEALTH
//=========================
//agrunt settings
#define AGRUNT_DMG_PUNCH		10
#define AGRUNT_HEALTH		100

//apache
#define APACHE_HEALTH		300

//barney
#define BARNEY_HEALTH		50 
#define DEAD_BARNEY_HEALTH		8

// HEALTH/SUIT CHARGE DISTRIBUTION
#define SUIT_CHARGER_CAP		75
#define HEATH_CHARGER_CAP		50
#define MEDKIT_CAP			20

#define BOLT_DMG			40
#define M203_DMG			100
#define BULLSQUID_HEALTH		40

#define HGRUNT_GRENADE_SPEED		400
#define HGRUNT_SHOTGUN_PELLETS	5
#define HGRUNT_DMG_KICK		5
#define HGRUNT_HEALTH		70
#define HASSASSIN_HEALTH		50

#define LEECH_HEALTH		2
#define LEECH_DMG_BITE		2

//headcrab
#define HEADCRAB_HEALTH	         	10
#define HEADCRAB_DMG_BITE	         	10

//scientist
#define SCIENTIST_HEALTH	         	20
#define SCIENTIST_HEAL	         	25

//turret
#define TURRET_HEALTH	         	60
#define MINITURRET_HEALTH	         	40
#define SENTRY_HEALTH	         	50

//zombie
#define ZOMBIE_HEALTH		75
#define ZOMBIE_ONE_SLASH		15
#define ZOMBIE_BOTH_SLASH               40

//hitgroup setttings 
#define DMG_HEAD		3
#define DMG_CHEST		1
#define DMG_STOMACH		1
#define DMG_ARM		1
#define DMG_LEG		1

//bullets damage
#define _9MM_DMG		10
#define _MP5_DMG		5
#define _12MM_DMG		20
#define _357_DMG		40
#define _556_DMG		30
#define _762_DMG		50		
#define BUCKSHOT_DMG	15

//bullet vector cone
#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )
#define VECTOR_CONE_0DEGREES	Vector( 0.00000, 0.00000, 0.00000 )
#endif//DEFAULT_H