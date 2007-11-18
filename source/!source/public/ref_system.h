//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_system.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

#include "ref_format.h" 
#include "version.h"

// time stamp formats
#define TIME_FULL		0
#define TIME_DATE_ONLY	1
#define TIME_TIME_ONLY	2
#define TIME_NO_SECONDS	3




#define MAX_LIGHTSTYLES	256

#define ENTITY_FLAGS	68
#define POWERSUIT_SCALE	4.0F

#define	EF_TELEPORTER	(1<<0)		// particle fountain
#define	EF_ROTATE		(1<<1)		// rotate (bonus items)

// shared client/renderer flags
#define	RF_MINLIGHT	1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL	2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL	4		// only draw through eyes
#define	RF_FULLBRIGHT	8		// allways draw full intensity
#define	RF_DEPTHHACK	16		// for view weapon Z crunching
#define	RF_TRANSLUCENT	32
#define	RF_FRAMELERP	64
#define	RF_BEAM		128
#define	RF_IR_VISIBLE	256		// skin is an index in image_precache
#define	RF_GLOW		512		// pulse lighting for bonus items

// render private flags
#define	RDF_NOWORLDMODEL	1		// used for player configuration screen
#define	RDF_IRGOGGLES	2
#define	RDF_PAIN           	4

// phys movetype
#define MOVETYPE_NONE		0	// never moves
#define MOVETYPE_NOCLIP		1	// origin and angles change with no interaction
#define MOVETYPE_PUSH		2	// no clip to world, push on box contact
#define MOVETYPE_WALK		3	// gravity
#define MOVETYPE_STEP		4	// gravity, special edge handling
#define MOVETYPE_FLY		5
#define MOVETYPE_TOSS		6	// gravity
#define MOVETYPE_BOUNCE		7
#define MOVETYPE_FOLLOW		8	// attached models
#define MOVETYPE_CONVEYOR		9
#define MOVETYPE_PUSHABLE		10

enum 
{
	HOST_OFFLINE = 0,	// host_init( funcname *arg ) same much as:
	HOST_NORMAL,	// "host_shared"
	HOST_DEDICATED,	// "host_dedicated"
	HOST_EDITOR,	// "host_editor"
	BSPLIB,		// "bsplib"
	IMGLIB,		// "imglib"
	QCCLIB,		// "qcclib"
	ROQLIB,		// "roqlib"
	SPRITE,		// "sprite"
	STUDIO,		// "studio"
	CREDITS,		// "splash"
	HOST_INSTALL,	// "install"
};

enum ai_activity
{
	ACT_RESET = 0,	// Set m_Activity to this invalid value to force a reset to m_IdealActivity
	ACT_IDLE = 1,
	ACT_GUARD,
	ACT_WALK,
	ACT_RUN,
	ACT_FLY,		// Fly (and flap if appropriate)
	ACT_SWIM,
	ACT_HOP,		// vertical jump
	ACT_LEAP,		// long forward jump
	ACT_FALL,
	ACT_LAND,
	ACT_STRAFE_LEFT,
	ACT_STRAFE_RIGHT,
	ACT_ROLL_LEFT,	// tuck and roll, left
	ACT_ROLL_RIGHT,	// tuck and roll, right
	ACT_TURN_LEFT,	// turn quickly left (stationary)
	ACT_TURN_RIGHT,	// turn quickly right (stationary)
	ACT_CROUCH,	// the act of crouching down from a standing position
	ACT_CROUCHIDLE,	// holding body in crouched position (loops)
	ACT_STAND,	// the act of standing from a crouched position
	ACT_USE,
	ACT_SIGNAL1,
	ACT_SIGNAL2,
	ACT_SIGNAL3,
	ACT_TWITCH,
	ACT_COWER,
	ACT_SMALL_FLINCH,
	ACT_BIG_FLINCH,
	ACT_RANGE_ATTACK1,
	ACT_RANGE_ATTACK2,
	ACT_MELEE_ATTACK1,
	ACT_MELEE_ATTACK2,
	ACT_RELOAD,
	ACT_ARM,		// pull out gun, for instance
	ACT_DISARM,	// reholster gun
	ACT_EAT,		// monster chowing on a large food item (loop)
	ACT_DIESIMPLE,
	ACT_DIEBACKWARD,
	ACT_DIEFORWARD,
	ACT_DIEVIOLENT,
	ACT_BARNACLE_HIT,	// barnacle tongue hits a monster
	ACT_BARNACLE_PULL,	// barnacle is lifting the monster ( loop )
	ACT_BARNACLE_CHOMP,	// barnacle latches on to the monster
	ACT_BARNACLE_CHEW,	// barnacle is holding the monster in its mouth ( loop )
	ACT_SLEEP,
	ACT_INSPECT_FLOOR,	// for active idles, look at something on or near the floor
	ACT_INSPECT_WALL,	// for active idles, look at something directly ahead of you
	ACT_IDLE_ANGRY,	// alternate idle animation in which the monster is clearly agitated. (loop)
	ACT_WALK_HURT,	// limp  (loop)
	ACT_RUN_HURT,	// limp  (loop)
	ACT_HOVER,	// Idle while in flight
	ACT_GLIDE,	// Fly (don't flap)
	ACT_FLY_LEFT,	// Turn left in flight
	ACT_FLY_RIGHT,	// Turn right in flight
	ACT_DETECT_SCENT,	// this means the monster smells a scent carried by the air
	ACT_SNIFF,	// this is the act of actually sniffing an item in front of the monster
	ACT_BITE,		// some large monsters can eat small things in one bite. This plays one time, EAT loops.
	ACT_THREAT_DISPLAY,	// without attacking, monster demonstrates that it is angry. (Yell, stick out chest, etc )
	ACT_FEAR_DISPLAY,	// monster just saw something that it is afraid of
	ACT_EXCITED,	// for some reason, monster is excited. Sees something he really likes to eat, or whatever
	ACT_SPECIAL_ATTACK1,// very monster specific special attacks.
	ACT_SPECIAL_ATTACK2,	
	ACT_COMBAT_IDLE,	// agitated idle.
	ACT_WALK_SCARED,
	ACT_RUN_SCARED,
	ACT_VICTORY_DANCE,	// killed a player, do a victory dance.
	ACT_DIE_HEADSHOT,	// die, hit in head. 
	ACT_DIE_CHESTSHOT,	// die, hit in chest
	ACT_DIE_GUTSHOT,	// die, hit in gut
	ACT_DIE_BACKSHOT,	// die, hit in back
	ACT_FLINCH_HEAD,
	ACT_FLINCH_CHEST,
	ACT_FLINCH_STOMACH,
	ACT_FLINCH_LEFTARM,
	ACT_FLINCH_RIGHTARM,
	ACT_FLINCH_LEFTLEG,
	ACT_FLINCH_RIGHTLEG,
	ACT_VM_NONE,	// weapon viewmodel animations
	ACT_VM_DEPLOY,	// deploy
	ACT_VM_DEPLOY_EMPTY,// deploy empty weapon
	ACT_VM_HOLSTER,	// holster empty weapon
	ACT_VM_HOLSTER_EMPTY,
	ACT_VM_IDLE1,
	ACT_VM_IDLE2,
	ACT_VM_IDLE3,
	ACT_VM_RANGE_ATTACK1,
	ACT_VM_RANGE_ATTACK2,
	ACT_VM_RANGE_ATTACK3,
	ACT_VM_MELEE_ATTACK1,
	ACT_VM_MELEE_ATTACK2,
	ACT_VM_MELEE_ATTACK3,
	ACT_VM_SHOOT_EMPTY,
	ACT_VM_START_RELOAD,
	ACT_VM_RELOAD,
	ACT_VM_RELOAD_EMPTY,
	ACT_VM_TURNON,
	ACT_VM_TURNOFF,
	ACT_VM_PUMP,	// pumping gun
	ACT_VM_PUMP_EMPTY,
	ACT_VM_START_CHARGE,
	ACT_VM_CHARGE,
	ACT_VM_OVERLOAD,
	ACT_VM_IDLE_EMPTY,
};

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings, same as MsgWarn
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_LOAD,		// "-dev 4", show messages about loading resources
	D_NOTE,		// "-dev 5", show system notifications for engine develeopers
	D_MEMORY,		// "-dev 6", show memory allocation
};

static activity_map_t activity_map[] =
{
{ACT_IDLE,		"ACT_IDLE"		},
{ACT_GUARD,		"ACT_GUARD"		},
{ACT_WALK,		"ACT_WALK"		},
{ACT_RUN,			"ACT_RUN"			},
{ACT_FLY,			"ACT_FLY"			},
{ACT_SWIM,		"ACT_SWIM",		},
{ACT_HOP,			"ACT_HOP",		},
{ACT_LEAP,		"ACT_LEAP"		},
{ACT_FALL,		"ACT_FALL"		},
{ACT_LAND,		"ACT_LAND"		},
{ACT_STRAFE_LEFT,		"ACT_STRAFE_LEFT"		},
{ACT_STRAFE_RIGHT,		"ACT_STRAFE_RIGHT"		},
{ACT_ROLL_LEFT,		"ACT_ROLL_LEFT"		},
{ACT_ROLL_RIGHT,		"ACT_ROLL_RIGHT"		},
{ACT_TURN_LEFT,		"ACT_TURN_LEFT"		},
{ACT_TURN_RIGHT,		"ACT_TURN_RIGHT"		},
{ACT_CROUCH,		"ACT_CROUCH"		},
{ACT_CROUCHIDLE,		"ACT_CROUCHIDLE"		},
{ACT_STAND,		"ACT_STAND"		},
{ACT_USE,			"ACT_USE"			},
{ACT_SIGNAL1,		"ACT_SIGNAL1"		},
{ACT_SIGNAL2,		"ACT_SIGNAL2"		},
{ACT_SIGNAL3,		"ACT_SIGNAL3"		},
{ACT_TWITCH,		"ACT_TWITCH"		},
{ACT_COWER,		"ACT_COWER"		},
{ACT_SMALL_FLINCH,		"ACT_SMALL_FLINCH"		},
{ACT_BIG_FLINCH,		"ACT_BIG_FLINCH"		},
{ACT_RANGE_ATTACK1,		"ACT_RANGE_ATTACK1"		},
{ACT_RANGE_ATTACK2,		"ACT_RANGE_ATTACK2"		},
{ACT_MELEE_ATTACK1,		"ACT_MELEE_ATTACK1"		},
{ACT_MELEE_ATTACK2,		"ACT_MELEE_ATTACK2"		},
{ACT_RELOAD,		"ACT_RELOAD"		},
{ACT_ARM,			"ACT_ARM"			},
{ACT_DISARM,		"ACT_DISARM"		},
{ACT_EAT,			"ACT_EAT"			},
{ACT_DIESIMPLE,		"ACT_DIESIMPLE"		},
{ACT_DIEBACKWARD,		"ACT_DIEBACKWARD"		},
{ACT_DIEFORWARD,		"ACT_DIEFORWARD"		},
{ACT_DIEVIOLENT,		"ACT_DIEVIOLENT"		},
{ACT_BARNACLE_HIT,		"ACT_BARNACLE_HIT"		},
{ACT_BARNACLE_PULL,		"ACT_BARNACLE_PULL"		},
{ACT_BARNACLE_CHOMP,	"ACT_BARNACLE_CHOMP"	},
{ACT_BARNACLE_CHEW,		"ACT_BARNACLE_CHEW"		},
{ACT_SLEEP,		"ACT_SLEEP"		},
{ACT_INSPECT_FLOOR,		"ACT_INSPECT_FLOOR"		},
{ACT_INSPECT_WALL,		"ACT_INSPECT_WALL"		},
{ACT_IDLE_ANGRY,		"ACT_IDLE_ANGRY"		},
{ACT_WALK_HURT,		"ACT_WALK_HURT"		},
{ACT_RUN_HURT,		"ACT_RUN_HURT"		},
{ACT_HOVER,		"ACT_HOVER"		},
{ACT_GLIDE,		"ACT_GLIDE"		},
{ACT_FLY_LEFT,		"ACT_FLY_LEFT"		},
{ACT_FLY_RIGHT,		"ACT_FLY_RIGHT"		},
{ACT_DETECT_SCENT,		"ACT_DETECT_SCENT"		},
{ACT_SNIFF,		"ACT_SNIFF"		},		
{ACT_BITE,		"ACT_BITE"		},		
{ACT_THREAT_DISPLAY,	"ACT_THREAT_DISPLAY"	},
{ACT_FEAR_DISPLAY,		"ACT_FEAR_DISPLAY"		},
{ACT_EXCITED,		"ACT_EXCITED"		},	
{ACT_SPECIAL_ATTACK1,	"ACT_SPECIAL_ATTACK1"	},
{ACT_SPECIAL_ATTACK2,	"ACT_SPECIAL_ATTACK2"	},
{ACT_COMBAT_IDLE,		"ACT_COMBAT_IDLE"		},
{ACT_WALK_SCARED,		"ACT_WALK_SCARED"		},
{ACT_RUN_SCARED,		"ACT_RUN_SCARED"		},
{ACT_VICTORY_DANCE,		"ACT_VICTORY_DANCE"		},
{ACT_DIE_HEADSHOT,		"ACT_DIE_HEADSHOT"		},
{ACT_DIE_CHESTSHOT,		"ACT_DIE_CHESTSHOT"		},
{ACT_DIE_GUTSHOT,		"ACT_DIE_GUTSHOT"		},
{ACT_DIE_BACKSHOT,		"ACT_DIE_BACKSHOT"		},
{ACT_FLINCH_HEAD,		"ACT_FLINCH_HEAD"		},
{ACT_FLINCH_CHEST,		"ACT_FLINCH_CHEST"		},
{ACT_FLINCH_STOMACH,	"ACT_FLINCH_STOMACH"	},
{ACT_FLINCH_LEFTARM,	"ACT_FLINCH_LEFTARM"	},
{ACT_FLINCH_RIGHTARM,	"ACT_FLINCH_RIGHTARM"	},
{ACT_FLINCH_LEFTLEG,	"ACT_FLINCH_LEFTLEG"	},
{ACT_FLINCH_RIGHTLEG,	"ACT_FLINCH_RIGHTLEG"	},
{ACT_VM_NONE,		"ACT_VM_NONE"		},	// invalid animation
{ACT_VM_DEPLOY,		"ACT_VM_DEPLOY"		},	// deploy
{ACT_VM_DEPLOY_EMPTY,	"ACT_VM_DEPLOY_EMPTY"	},	// deploy empty weapon
{ACT_VM_HOLSTER,		"ACT_VM_HOLSTER"		},	// holster empty weapon
{ACT_VM_HOLSTER_EMPTY,	"ACT_VM_HOLSTER_EMPTY"	},
{ACT_VM_IDLE1,		"ACT_VM_IDLE1"		},
{ACT_VM_IDLE2,		"ACT_VM_IDLE2"		},
{ACT_VM_IDLE3,		"ACT_VM_IDLE3"		},
{ACT_VM_RANGE_ATTACK1,	"ACT_VM_RANGE_ATTACK1"	},
{ACT_VM_RANGE_ATTACK2,	"ACT_VM_RANGE_ATTACK2"	},
{ACT_VM_RANGE_ATTACK3,	"ACT_VM_RANGE_ATTACK3"	},
{ACT_VM_MELEE_ATTACK1,	"ACT_VM_MELEE_ATTACK1"	},
{ACT_VM_MELEE_ATTACK2,	"ACT_VM_MELEE_ATTACK2"	},
{ACT_VM_MELEE_ATTACK3,	"ACT_VM_MELEE_ATTACK3"	},
{ACT_VM_SHOOT_EMPTY,	"ACT_VM_SHOOT_EMPTY"	},
{ACT_VM_START_RELOAD,	"ACT_VM_START_RELOAD"	},
{ACT_VM_RELOAD,		"ACT_VM_RELOAD"		},
{ACT_VM_RELOAD_EMPTY,	"ACT_VM_RELOAD_EMPTY"	},
{ACT_VM_TURNON,		"ACT_VM_TURNON"		},
{ACT_VM_TURNOFF,		"ACT_VM_TURNOFF"		},
{ACT_VM_PUMP,		"ACT_VM_PUMP"		},	// user animations
{ACT_VM_PUMP_EMPTY,		"ACT_VM_PUMP_EMPTY"		},
{ACT_VM_START_CHARGE,	"ACT_VM_START_CHARGE"	},
{ACT_VM_CHARGE,		"ACT_VM_CHARGE"		},
{ACT_VM_OVERLOAD,		"ACT_VM_CHARGE"		},
{ACT_VM_IDLE_EMPTY,		"ACT_VM_IDLE_EMPTY"		},
{0, 			NULL			},
};

typedef struct search_s
{
	int	numfilenames;
	char	**filenames;
	char	*filenamesbuffer;

} search_t;

typedef struct physdata_s
{
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;	// "omega" in newton
	vec3_t		mins;		// for calculate size 
	vec3_t		maxs;		// and setup offset matrix

	NewtonCollision	*collision;
	NewtonBody	*physbody;	// ptr to physic body
} physdata_t;

typedef struct gameinfo_s
{
	//filesystem info
	char	basedir[128];
	char	gamedir[128];
	char	title[128];
          float	version;
	
	int	viewmode;
	int	gamemode;

	// system info
	int	cpunum;
	int64	tickcount;
	float	cpufreq;
	bool	rdtsc;
	
	char key[16];

} gameinfo_t;

typedef struct dll_info_s
{
	// generic interface
	const char	*name;		// library name
	const dllfunc_t	*fcts;		// list of dll exports	
	const char	*entry;		// entrypoint name (internal libs only)
	void		*link;		// hinstance of loading library

	// xash dlls entrypoint
	void		*(*main)( void*, void* );
	bool		crash;		// crash if dll not found

	// xash dlls validator
	size_t		api_size;		// generic interface size

} dll_info_t;

#define CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	2	// added to userinfo  when changed
#define CVAR_SERVERINFO	4	// added to serverinfo when changed
#define CVAR_SYSTEMINFO	8	// these cvars will be duplicated on all clients
#define CVAR_INIT		16	// don't allow change from console at all, but can be set from the command line
#define CVAR_LATCH		32	// save changes until server restart
#define CVAR_READ_ONLY	64	// display only, cannot be set by user at all
#define CVAR_USER_CREATED	128	// created by a set command (prvm used)
#define CVAR_TEMP		256	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT		512	// can not be changed if cheats are disabled
#define CVAR_NORESTART	1024	// do not clear when a cvar_restart is issued
#define CVAR_MAXFLAGSVAL	2047	// maximum number of flags

typedef struct cvar_s
{
	char	*name;
	char	*reset_string;	// cvar_restart will reset to this value
	char	*latched_string;	// for CVAR_LATCH vars
	char	*description;	// variable descrition info
	int	flags;		// state flags
	bool	modified;		// set each time the cvar is changed
	int	modificationCount;	// incremented each time the cvar is changed

	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )

	struct cvar_s *next;
	struct cvar_s *hash;
} cvar_t;

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;

} dlight_t;

typedef struct particle_s
{
	vec3_t	origin;
	int	color;
	float	alpha;

} particle_t;

typedef struct lightstyle_s
{
	float	rgb[3];		// 0.0 - 2.0
	float	white;		// highest of rgb

} lightstyle_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	int		sequence;
	float		frame;

	byte		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct entity_s
{
	model_t		*model;		// opaque type outside refresh
	model_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		//previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// also used as RF_BEAM's "from"
	float		oldorigin[3];	// also used as RF_BEAM's "to"

          float		animtime;	
	float		frame;		// also used as RF_BEAM's diameter
	float		framerate;

	int		body;
	int		skin;
	
	byte		blending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];
	byte		mouth;		//TODO: move to struct
	
          int		movetype;		//entity moving type
	int		sequence;
	float		scale;
	
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	int		skinnum;		// also used as RF_BEAM's palette index

	int		lightstyle;	// for flashing entities
	float		alpha;		// ignore if RF_TRANSLUCENT isn't set

	image_t		*image;		// NULL for inline skin
	int		flags;

} entity_t;

typedef struct
{
	int		x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];		// rgba 0-1 full screen blend
	float		time;		// time is used to auto animate
	int		rdflags;		// RDF_UNDERWATER, etc

	byte		*areabits;	// if not NULL, only areas with set bits will be drawn

	lightstyle_t	*lightstyles;	// [MAX_LIGHTSTYLES]

	int		num_entities;
	entity_t		*entities;

	int		num_dlights;
	dlight_t		*dlights;

	int		num_particles;
	particle_t	*particles;

} refdef_t;

/*
==============================================================================

GENERIC INTERFACE
==============================================================================
*/
typedef struct generic_api_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(*_api_t)

} generic_api_t;

/*
==============================================================================

STDLIB SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	//interface validator
	size_t	api_size;				// must matched with sizeof(stdlib_api_t)
	
	// base events
	void (*print)( const char *msg );		// basic text message
	void (*printf)( const char *msg, ... );		// formatted text message
	void (*dprintf)( int level, const char *msg, ...);// developer text message
	void (*wprintf)( const char *msg, ... );	// warning text message
	void (*error)( const char *msg, ... );		// abnormal termination with message
	void (*exit)( void );			// normal silent termination
	char *(*input)( void );			// system console input	
	void (*sleep)( int msec );			// sleep for some msec
	char *(*clipboard)( void );			// get clipboard data
	uint (*keyevents)( void );			// peek windows message

	// crclib.c funcs
	void (*crc_init)(word *crcvalue);			// set initial crc value
	word (*crc_block)(byte *start, int count);		// calculate crc block
	void (*crc_process)(word *crcvalue, byte data);		// process crc byte
	byte (*crc_sequence)(byte *base, int length, int sequence);	// calculate crc for sequence

	// memlib.c funcs
	void (*memcpy)(void *dest, void *src, size_t size, const char *file, int line);
	void (*memset)(void *dest, int set, size_t size, const char *file, int line);
	void *(*realloc)(byte *pool, void *mem, size_t size, const char *file, int line);
	void (*move)(byte *pool, void **dest, void *src, size_t size, const char *file, int line); // not a memmove
	void *(*malloc)(byte *pool, size_t size, const char *file, int line);
	void (*free)(void *data, const char *file, int line);

	// xash memlib extension - memory pools
	byte *(*mallocpool)(const char *name, const char *file, int line);
	void (*freepool)(byte **poolptr, const char *file, int line);
	void (*clearpool)(byte *poolptr, const char *file, int line);
	void (*memcheck)(const char *file, int line);		// check memory pools for consistensy

	// common functions
	void (*Com_InitRootDir)( char *path );			// init custom rootdir 
	void (*Com_LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*Com_AddGameHierarchy)(const char *dir);		// add base directory in search list
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	bool (*Com_GetParm)( char *parm, char *out );		// get parm from cmdline
	void (*Com_FileBase)(char *in, char *out);		// get filename without path & ext
	bool (*Com_FileExists)(const char *filename);		// return true if file exist
	long (*Com_FileSize)(const char *filename);		// same as Com_FileExists but return filesize
	const char *(*Com_FileExtension)(const char *in);		// return extension of file
	const char *(*Com_RemovePath)(const char *in);		// return file without path
	void (*Com_StripExtension)(char *path);			// remove extension if present
	void (*Com_StripFilePath)(const char* const src, char* dst);// get file path without filename.ext
	void (*Com_DefaultExtension)(char *path, const char *ext );	// append extension if not present
	void (*Com_ClearSearchPath)( void );			// delete all search pathes
	void (*Com_CreateThread)(int, bool, void(*fn)(int));	// run individual thread
	void (*Com_ThreadLock)( void );			// lock current thread
	void (*Com_ThreadUnlock)( void );			// unlock numthreads
	int (*Com_NumThreads)( void );			// returns count of active threads
	bool (*Com_LoadScript)(const char *name,char *buf,int size);// load script into stack from file or bufer
	bool (*Com_AddScript)(const char *name,char *buf, int size);// include script from file or buffer
	void (*Com_ResetScript)( void );			// reset current script state
	char *(*Com_ReadToken)( bool newline );			// get next token on a line or newline
	bool (*Com_TryToken)( void );				// return 1 if have token on a line 
	void (*Com_FreeToken)( void );			// free current token to may get it again
	void (*Com_SkipToken)( void );			// skip current token and jump into newline
	bool (*Com_MatchToken)( const char *match );		// compare current token with user keyword
	char *(*Com_ParseToken)(const char **data );		// parse token from char buffer
	char *(*Com_ParseWord)( const char **data );		// parse word from char buffer
	search_t *(*Com_Search)(const char *pattern, int casecmp );	// returned list of found files
	bool (*Com_Filter)(char *filter, char *name, int casecmp ); // compare keyword by mask with filter
	char *com_token;					// contains current token

	// real filesystem
	file_t *(*fopen)(const char* path, const char* mode);		// same as fopen
	int (*fclose)(file_t* file);					// same as fclose
	long (*fwrite)(file_t* file, const void* data, size_t datasize);	// same as fwrite
	long (*fread)(file_t* file, void* buffer, size_t buffersize);	// same as fread, can see trough pakfile
	int (*fprint)(file_t* file, const char *msg);			// printed message into file		
	int (*fprintf)(file_t* file, const char* format, ...);		// same as fprintf
	int (*fgets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*fseek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*ftell)(file_t* file);					// like a ftell

	// virtual filesystem
	vfile_t *(*vfcreate)(byte *buffer, size_t buffsize);		// create virtual stream
	vfile_t *(*vfopen)(const char *filename, const char* mode);		// virtual fopen
	int (*vfclose)(vfile_t* file);				// free buffer or write dump
	long (*vfwrite)(vfile_t* file, const void* buf, size_t datasize);	// write into buffer
	long (*vfwrite2)(vfile_t* handle, byte* buffer, size_t datasize);	// deflate and write into buffer
	long (*vfread)(vfile_t* file, void* buffer, size_t buffersize);	// read from buffer
	int (*vfseek)(vfile_t* file, fs_offset_t offset, int whence);	// fseek, can seek in packfiles too
	bool (*vfunpack)( void* comp, size_t size1, void **buf, size_t size2);// deflate zipped buffer
	long (*vftell)(vfile_t* file);				// like a ftell

	// filesystem simply user interface
	byte *(*Com_LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*Com_WriteFile)(const char *path, void *data, long len);	// write file into disk
	rgbdata_t *(*Com_LoadImage)(const char *path, char *data, int size );	// extract image into rgba buffer
	void (*Com_SaveImage)(const char *filename, rgbdata_t *buffer );	// save image into specified format
	bool (*Com_ProcessImage)( const char *name, rgbdata_t **pix );	// convert and resample image
	void (*Com_FreeImage)( rgbdata_t *pack );			// free image buffer
	bool (*Com_LoadLibrary)( dll_info_t *dll );			// load library 
	bool (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer

	// stdlib.c funcs
	void (*strnupr)(const char *in, char *out, size_t size_out);// convert string to upper case
	void (*strnlwr)(const char *in, char *out, size_t size_out);// convert string to lower case
	void (*strupr)(const char *in, char *out);		// convert string to upper case
	void (*strlwr)(const char *in, char *out);		// convert string to lower case
	int (*strlen)( const char *string );			// returns string real length
	int (*cstrlen)( const char *string );			// return string length without color prefixes
	char (*toupper)(const char in );			// convert one charcster to upper case
	char (*tolower)(const char in );			// convert one charcster to lower case
	size_t (*strncat)(char *dst, const char *src, size_t n);	// add new string at end of buffer
	size_t (*strcat)(char *dst, const char *src);		// add new string at end of buffer
	size_t (*strncpy)(char *dst, const char *src, size_t n);	// copy string to existing buffer
	size_t (*strcpy)(char *dst, const char *src);		// copy string to existing buffer
	char *(*stralloc)(const char *in,const char *file,int line);// create buffer and copy string here
	int (*atoi)(const char *str);				// convert string to integer
	float (*atof)(const char *str);			// convert string to float
	void (*atov)( float *dst, const char *src, size_t n );	// convert string to vector
	char *(*strchr)(const char *s, char c);			// find charcster in string at left side
	char *(*strrchr)(const char *s, char c);		// find charcster in string at right side
	int (*strnicmp)(const char *s1, const char *s2, int n);	// compare strings with case insensative
	int (*stricmp)(const char *s1, const char *s2);		// compare strings with case insensative
	int (*strncmp)(const char *s1, const char *s2, int n);	// compare strings with case sensative
	int (*strcmp)(const char *s1, const char *s2);		// compare strings with case sensative
	char *(*stristr)( const char *s1, const char *s2 );	// find s2 in s1 with case insensative
	char *(*strstr)( const char *s1, const char *s2 );	// find s2 in s1 with case sensative
	size_t (*strpack)( byte *buf, size_t pos, char *s1, int n );// include string into buffer (same as strncat)
	size_t (*strunpack)( byte *buf, size_t pos, char *s1 );	// extract string from buffer
	int (*vsprintf)(char *buf, const char *fmt, va_list args);	// format message
	int (*sprintf)(char *buffer, const char *format, ...);	// print into buffer
	char *(*va)(const char *format, ...);			// print into temp buffer
	int (*vsnprintf)(char *buf, size_t size, const char *fmt, va_list args);	// format message
	int (*snprintf)(char *buffer, size_t buffersize, const char *format, ...);	// print into buffer
	const char* (*timestamp)( int format );			// returns current time stamp
	
	// misc utils	
	gameinfo_t *GameInfo;				// user game info (filled by engine)
	char com_TXcommand;					// quark command (get rid of this)

} stdlib_api_t;

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( uint funcname, int argc, char **argv ); // init host
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host

} launch_exp_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/

typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_exp_t)

	// initialize
	bool (*Init)( void *hInstance, void *WndProc );	// init all render systems
	void (*Shutdown)( void );	// shutdown all render systems
	void (*AppActivate)( bool activate );		// ??

	void	(*BeginRegistration) (char *map);
	model_t	*(*RegisterModel) (char *name);
	image_t	*(*RegisterSkin) (char *name);
	image_t	*(*RegisterPic) (char *name);
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration) (void);

	void	(*BeginFrame)( void );
	void	(*RenderFrame) (refdef_t *fd);
	void	(*EndFrame)( void );

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, bool force_gamma ); // write screenshot with same name 
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *name);

	// get rid of this
	void	(*DrawGetPicSize) (int *w, int *h, char *name);	// will return 0 0 if not found

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_imp_t)

	void	(*Cmd_AddCommand)( const char *name, xcommand_t cmd, const char *cmd_desc );
	void	(*Cmd_RemoveCommand)( char *name );
	int	(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);
	void	(*Cmd_ExecuteText) (int exec_when, char *text);

	// client fundamental callbacks
	void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );
	void	(*ShowCollision)( void );// debug

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags, const char *desc );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue)( const char *name, float value );

	bool	(*Vid_GetModeInfo)( int *width, int *height, int mode );
	void	(*Vid_MenuInit)( void );
	void	(*Vid_NewWindow)( int width, int height );

} render_imp_t;

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)

	// initialize
	bool (*Init)( void );	// init all physic systems
	void (*Shutdown)( void );	// shutdown all render systems

	void (*LoadBSP)( uint *buf );	// generate tree collision
	void (*FreeBSP)( void );	// release tree collision
	void (*ShowCollision)( void );// debug
	void (*Frame)( float time );	// physics frame

	// simple objects
	void (*CreateBOX)( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, NewtonCollision **newcol, NewtonBody **newbody );
	void (*RemoveBOX)( NewtonBody *body );

} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*Transform)( sv_edict_t *ed, vec3_t origin, vec3_t angles );

} physic_imp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );

#endif//REF_SYSTEM_H