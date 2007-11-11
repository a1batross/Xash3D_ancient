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

// bsplib compile flags
#define BSP_ONLYENTS	0x01
#define BSP_ONLYVIS		0x02
#define BSP_ONLYRAD		0x04
#define BSP_FULLCOMPILE	0x08

// qcclib compile flags
#define QCC_PROGDEFS	0x01
#define QCC_OPT_LEVEL_0	0x02
#define QCC_OPT_LEVEL_1	0x04
#define QCC_OPT_LEVEL_2	0x08
#define QCC_OPT_LEVEL_3	0x10

#define MAX_DLIGHTS		32
#define MAX_ENTITIES	128
#define MAX_PARTICLES	4096
#define MAX_LIGHTSTYLES	256

#define ENTITY_FLAGS	68
#define POWERSUIT_SCALE	4.0F

#define SHELL_RED_COLOR	0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR	0xDC
#define SHELL_RB_COLOR	0x68
#define SHELL_BG_COLOR	0x78
#define SHELL_WHITE_COLOR	0xD7

// shared client/renderer flags
#define	RF_MINLIGHT	1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL	2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL	4		// only draw through eyes
#define	RF_FULLBRIGHT	8		// allways draw full intensity
#define	RF_DEPTHHACK	16		// for view weapon Z crunching
#define	RF_TRANSLUCENT	32
#define	RF_FRAMELERP	64
#define	RF_BEAM		128
#define	RF_CUSTOMSKIN	256		// skin is an index in image_precache
#define	RF_GLOW		512		// pulse lighting for bonus items
#define	RF_SHELL_RED	1024
#define	RF_SHELL_GREEN	2048
#define	RF_SHELL_BLUE	4096
#define	RF_IR_VISIBLE	0x00008000	// 32768
#define	RF_SHELL_DOUBLE	0x00010000	// 65536
#define	RF_SHELL_HALF_DAM	0x00020000
#define	RF_USE_DISGUISE	0x00040000

// render private flags
#define	RDF_UNDERWATER	1	// warp the screen as apropriate
#define	RDF_NOWORLDMODEL	2	// used for player configuration screen
#define	RDF_IRGOGGLES	4
#define	RDF_UVGOGGLES	8
#define	RDF_BLOOM		32 
#define	RDF_PAIN           	64
#define	RDF_WATER		128
#define	RDF_LAVA		256
#define	RDF_SLIME		512

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

// opengl mask
#define GL_COLOR_INDEX	0x1900
#define GL_STENCIL_INDEX	0x1901
#define GL_DEPTH_COMPONENT	0x1902
#define GL_RED		0x1903
#define GL_GREEN		0x1904
#define GL_BLUE		0x1905
#define GL_ALPHA		0x1906
#define GL_RGB		0x1907
#define GL_RGBA		0x1908
#define GL_LUMINANCE	0x1909
#define GL_LUMINANCE_ALPHA	0x190A
#define GL_BGR		0x80E0
#define GL_BGRA		0x80E1

// gl data type
#define GL_BYTE		0x1400
#define GL_UNSIGNED_BYTE	0x1401
#define GL_SHORT		0x1402
#define GL_UNSIGNED_SHORT	0x1403
#define GL_INT		0x1404
#define GL_UNSIGNED_INT	0x1405
#define GL_FLOAT		0x1406
#define GL_2_BYTES		0x1407
#define GL_3_BYTES		0x1408
#define GL_4_BYTES		0x1409
#define GL_DOUBLE		0x140A

enum comp_format
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// studio model skins
	PF_INDEXED_32,	// sprite 32-bit palette
	PF_RGBA_32,	// already prepared ".bmp", ".tga" or ".jpg" image 
	PF_ARGB_32,	// uncompressed dds image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_RGB_24_FLIP,	// flip image for screenshots
	PF_DXT1,		// nvidia DXT1 format
	PF_DXT2,		// nvidia DXT2 format
	PF_DXT3,		// nvidia DXT3 format
	PF_DXT4,		// nvidia DXT4 format
	PF_DXT5,		// nvidia DXT5 format
	PF_ATI1N,		// ati 1N texture
	PF_ATI2N,		// ati 2N texture
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_RXGB,		// doom3 normal maps
	PF_ABGR_64,	// uint image
	PF_RGBA_GN,	// internal generated texture
	PF_TOTALCOUNT,	// must be last
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

typedef enum
{
	MSG_ONE,
	MSG_ALL,
	MSG_PHS,
	MSG_PVS,
	MSG_ONE_R,	// reliable
	MSG_ALL_R,
	MSG_PHS_R,
	MSG_PVS_R,
} msgtype_t;

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings, same as MsgWarn
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_LOAD,		// "-dev 4", show messages about loading resources
	D_NOTE,		// "-dev 5", show system notifications for engine develeopers
};

// format info table
typedef struct
{
	int	format;	// pixelformat
	char	name[8];	// used for debug
	uint	glmask;	// RGBA mask
	uint	gltype;	// open gl datatype
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
	int	bpc;	// sizebytes (byte, short, float)
	int	block;	// blocksize < 0 needs alternate calc
} bpc_desc_t;

static bpc_desc_t PFDesc[] =
{
{PF_UNKNOWN,	"raw",	GL_RGBA,		GL_UNSIGNED_BYTE, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1,  0 },// expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  0 },
{PF_RGBA_32,	"RGBA 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_RGB_24,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_RGB_24_FLIP,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_DXT1,		"DXT1",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  8 },
{PF_DXT2,		"DXT2",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT3,		"DXT3",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT4,		"DXT4",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT5,		"DXT5",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_ATI1N,	"ATI1N",	GL_RGBA,		GL_UNSIGNED_BYTE, 1,  1,  8 },
{PF_ATI2N,	"3DC",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	GL_LUMINANCE,	GL_UNSIGNED_BYTE, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", GL_LUMINANCE,	GL_UNSIGNED_BYTE, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, 2,  1, -2 },
{PF_RXGB,		"RXGB",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_ABGR_64,	"ABGR 64",GL_BGRA,		GL_UNSIGNED_BYTE, 4,  2, -8 },
{PF_RGBA_GN,	"system",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
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

#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips
#define IMAGE_CUBEMAP_FLIP	0x00000010	// it's a cubemap with flipped sides( dds pack )

#define CUBEMAP_POSITIVEX	0x00000400L
#define CUBEMAP_NEGATIVEX	0x00000800L
#define CUBEMAP_POSITIVEY	0x00001000L
#define CUBEMAP_NEGATIVEY	0x00002000L
#define CUBEMAP_POSITIVEZ	0x00004000L
#define CUBEMAP_NEGATIVEZ	0x00008000L

typedef struct search_s
{
	int	numfilenames;
	char	**filenames;
	char	*filenamesbuffer;

} search_t;

typedef struct rgbdata_s
{
	word	width;		// image width
	word	height;		// image height
	byte	numLayers;	// multi-layer volume
	byte	numMips;		// mipmap count
	byte	bitsCount;	// RGB bits count
	uint	type;		// compression type
	uint	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	uint	size;		// for bounds checking
} rgbdata_t;

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
	void		*(*main)( void* );	// point type (e.g. common_t)
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

// pmove_state_t is the information necessary for client side movement
#define PM_NORMAL		0 // can accelerate and turn
#define PM_SPECTATOR	1
#define PM_DEAD		2 // no acceleration or turning
#define PM_GIB		3 // different bounding box
#define PM_FREEZE		4

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct
{
	byte		pm_type;
	vec3_t		origin;		// 12.3
	vec3_t		velocity;		// 12.3
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms
	short		gravity;
	vec3_t		delta_angles;	// add to command angles to get view direction
					// changed by spawns, rotating objects, and teleporters
} pmove_state_t;

typedef struct
{
	pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise
	vec3_t		viewangles;	// for fixed views
	vec3_t		viewoffset;	// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
					// set by weapon kicks, pain effects, etc

	vec3_t		gunangles;
	vec3_t		gunoffset;
	int		gunindex;
	int		gunframe;		// studio frame
	int		sequence;		// stuido animation sequence
	int		gunbody;
	int		gunskin; 

	float		blend[4];		// rgba full screen effect
	
	float		fov;		// horizontal field of view
	int		rdflags;		// refdef flags
	short		stats[32];	// fast status bar updates
} player_state_t;

// network protocol
typedef struct entity_state_s
{
	uint		number;		// edict index

	vec3_t		origin;
	vec3_t		angles;
	vec3_t		old_origin;	// for lerping animation
	int		modelindex;
	int		soundindex;
	int		weaponmodel;

	short		skin;		// skin for studiomodels
	short		frame;		// % playback position in animation sequences (0..512)
	byte		body;		// sub-model selection for studiomodels
	byte		sequence;		// animation sequence (0 - 255)
	uint		effects;		// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;		// for client side prediction, 8*(bits 0-4) is x/y radius
					// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
					// gi.linkentity sets this properly
	int		event;		// impulse events -- muzzle flashes, footsteps, etc
					// events only go out for a single frame, they
					// are automatically cleared each frame
	float		alpha;		// alpha value
} entity_state_t;

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

FILESYSTEM ENGINE INTERFACE
==============================================================================
*/
typedef struct filesystem_api_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(filesystem_api_t)

	// base functions
	void (*FileBase)(char *in, char *out);			// get filename without path & ext
	bool (*FileExists)(const char *filename);		// return true if file exist
	long (*FileSize)(const char *filename);			// same as FileExists but return filesize
	const char *(*FileExtension)(const char *in);		// return extension of file
	const char *(*FileWithoutPath)(const char *in);		// return file without path
	void (*StripExtension)(char *path);			// remove extension if present
	void (*StripFilePath)(const char* const src, char* dst);	// get file path without filename.ext
	void (*DefaultExtension)(char *path, const char *ext );	// append extension if not present
	void (*ClearSearchPath)( void );			// delete all search pathes

	// built-in search interface
	search_t *(*Search)(const char *pattern, int casecmp );	// returned list of found files

	// file low-level operations
	file_t *(*Open)(const char* path, const char* mode);		// same as fopen
	int (*Close)(file_t* file);					// same as fclose
	long (*Write)(file_t* file, const void* data, size_t datasize);	// same as fwrite
	long (*Read)(file_t* file, void* buffer, size_t buffersize);	// same as fread, can see trough pakfile
	int (*Print)(file_t* file, const char *msg);			// printed message into file		
	int (*Printf)(file_t* file, const char* format, ...);		// same as fprintf
	int (*Gets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*Seek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*Tell)(file_t* file);					// like a ftell

	// fs simply user interface
	byte *(*LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*WriteFile)(const char *filename, void *data, long len);	// write file into disk

} filesystem_api_t;

typedef struct vfilesystem_api_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vfilesystem_api_t)

	// file low-level operations
	vfile_t *(*Create)(byte *buffer, size_t buffsize);		// create virtual stream
	vfile_t *(*Open)(const char *filename, const char* mode);		// virtual fopen
	int (*Close)(vfile_t* file);					// free buffer or write dump
	long (*Write)(vfile_t* file, const void* data, size_t datasize);	// write into buffer
	long (*Read)(vfile_t* file, void* buffer, size_t buffersize);	// read from buffer
	int (*Seek)(vfile_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*Tell)(vfile_t* file);					// like a ftell

} vfilesystem_api_t;

/*
==============================================================================

INFOSTRING MANAGER ENGINE INTERFACE
==============================================================================
*/
typedef struct infostring_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(infostring_api_t)

	void (*Print) (char *s);
	bool (*Validate) (char *s);
	void (*RemoveKey) (char *s, char *key);
	char *(*ValueForKey) (char *s, char *key);
	void (*SetValueForKey) (char *s, char *key, char *value);

} infostring_api_t;


/*
==============================================================================

PARSE STUFF SYSTEM INTERFACE
==============================================================================
*/
typedef struct scriptsystem_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(scriptsystem_api_t)

	//user interface
	bool (*Load)( const char *name, char *buf, int size );// load script into stack from file or bufer
	bool (*Include)( const char *name, char *buf, int size );	// include script from file or buffer
	void (*Reset)( void );				// reset current script state
	char *(*GetToken)( bool newline );			// get next token on a line or newline
	bool (*TryToken)( void );				// return 1 if have token on a line 
	void (*FreeToken)( void );				// free current token to may get it again
	void (*SkipToken)( void );				// skip current token and jump into newline
	bool (*MatchToken)( const char *match );		// compare current token with user keyword
	char *(*ParseToken)(const char **data );		// parse token from char buffer
	char *(*ParseWord)( const char **data );		// parse word from char buffer
	bool (*FilterToken)(char *filter, char *name, int casecmp);	// compare keyword by mask with filter
	char *Token;					// contains current token
	char g_TXcommand;					// quark command

} scriptsystem_api_t;

/*
==============================================================================

INTERNAL COMPILERS INTERFACE
==============================================================================
*/
typedef struct compilers_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(compilers_api_t)

	bool (*Studio)( byte *mempool, const char *name, byte parms );	// input name of qc-script
	bool (*Sprite)( byte *mempool, const char *name, byte parms );	// input name of qc-script
	bool (*Image)( byte *mempool, const char *name, byte parms );	// input name of image
	bool (*PrepareBSP)( const char *dir, const char *name, byte params );	// compile map in gamedir 
	bool (*BSP)( void );
	bool (*PrepareDAT)( const char *dir, const char *name, byte params );	// compile dat in gamedir 
	bool (*DAT)( void );
	bool (*DecryptDAT)( int complen, int len, int method, char *info, char **buffer); //unpacking dat
	bool (*PrepareROQ)( const char *dir, const char *name, byte params );	// compile roq in gamedir 
	bool (*ROQ)( void );
} compilers_api_t;

/*
==============================================================================

STDIO SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	//interface validator
	size_t	api_size;				// must matched with sizeof(stdlib_api_t)
	
	// base events
	void (*print)( char *msg );			// basic text message
	void (*printf)( char *msg, ... );		// formatted text message
	void (*dprintf)( int level, char *msg, ... );	// developer text message
	void (*wprintf)( char *msg, ... );		// warning text message
	void (*error)( char *msg, ... );		// abnormal termination with message
	void (*exit)( void );			// normal silent termination
	char *(*input)( void );			// system console input	
	void (*sleep)( int msec );			// sleep for some msec
	char *(*clipboard)( void );			// get clipboard data
	void (*create_thread)(int, bool, void(*fn)(int));	// run individual thread
	void (*thread_lock)( void );
	void (*thread_unlock)( void );
	int (*get_numthreads)( void );

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

	// path initialization
	void (*InitRootDir)( char *path );			// init custom rootdir 
	void (*LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*AddGameHierarchy)(const char *dir);		// add base directory in search list

	filesystem_api_t		Fs;			// filesystem
	vfilesystem_api_t		VFs;			// virtual filesystem

	// timelib.c funcs
	const char* (*time_stamp)( int format );		// returns current time stamp
	double (*gettime)( void );				// hi-res timer
	gameinfo_t *GameInfo;				// misc utils

	scriptsystem_api_t		Script;			// parselib.c

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
	char *(*stralloc)(const char *in);			// create buffer and copy string here
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
	
	// xash dll loading system
	bool (*LoadLibrary)( dll_info_t *dll );		// load library 
	bool (*FreeLibrary)( dll_info_t *dll );		// free library
	void*(*GetProcAddress)( dll_info_t *dll, const char* name ); // gpa

} stdlib_api_t;

/*
==============================================================================

CVAR SYSTEM INTERFACE
==============================================================================
*/
typedef struct cvar_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(cvar_api_t)

	//cvar_t *(*Register)(const char *name, const char *value, int flags, const char *description );
	//cvar_t *(*SetString)(const char *name, char *value);
	//void (*SetValue)(const char *name, float value);

} cvar_api_t;

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( char *funcname, int argc, char **argv ); // init host
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host

} launch_exp_t;

/*
==============================================================================

COMMON.DLL INTERFACE
==============================================================================
*/

typedef struct common_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(common_api_t)

	// initialize
	bool (*Init)( int argc, char **argv );	// init all common systems
	void (*Shutdown)( void );	// shutdown all common systems

	rgbdata_t *(*LoadImage)(const char *filename, char *data, int size );
	void (*SaveImage)(const char *filename, rgbdata_t *buffer );
	void (*FreeImage)( rgbdata_t *pack );

	// common systems
	compilers_api_t	Compile;
	infostring_api_t	Info;

} common_exp_t;

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

	void	(*RenderFrame) (refdef_t *fd);

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, bool force_gamma ); // write screenshot with same name 
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *name);

	// get rid of this
	void	(*DrawGetPicSize) (int *w, int *h, char *name);	// will return 0 0 if not found
	void	(*DrawPic)(int x, int y, char *name);
	void	(*DrawChar)(float x, float y, int c);
	void	(*DrawString) (int x, int y, char *str);
	void	(*DrawTileClear) (int x, int y, int w, int h, char *name);
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawFadeScreen) (void);


	// video mode and refresh state management entry points
	void	(*CinematicSetPalette)( const byte *palette);	// NULL = game palette
	void	(*BeginFrame)( void );
	void	(*EndFrame)( void );
} render_exp_t;

typedef struct render_imp_s
{
	// shared xash systems
	compilers_api_t	Compile;
	stdlib_api_t	Stdio;

	rgbdata_t	*(*LoadImage)(const char *filename, char *data, int size );
	void	(*SaveImage)(const char *filename, rgbdata_t *buffer );
	void	(*FreeImage)( rgbdata_t *pack );

	void	(*Cmd_AddCommand) (char *name, void(*cmd)(void));
	void	(*Cmd_RemoveCommand) (char *name);
	int	(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);
	void	(*Cmd_ExecuteText) (int exec_when, char *text);

	// client fundamental callbacks
	void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );
	void	(*ShowCollision)( void );// debug

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char	*(*gamedir)	( void );
	char	*(*title)		( void );

	cvar_t	*(*Cvar_Get) (char *name, char *value, int flags);
	void	(*Cvar_Set)( char *name, char *value );
	void	(*Cvar_SetValue)( char *name, float value );

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
	// shared xash systems
	compilers_api_t	Compile;
	stdlib_api_t	Stdio;

	void (*Transform)( sv_edict_t *ed, vec3_t origin, vec3_t angles );

} physic_imp_t;

// this is the only function actually exported at the linker level
typedef render_exp_t *(*render_t)( render_imp_t* );
typedef physic_exp_t *(*physic_t)( physic_imp_t* );
typedef common_exp_t *(*common_t)( stdlib_api_t* );
typedef launch_exp_t *(*launch_t)( stdlib_api_t* );

#endif//REF_SYSTEM_H