//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        stdref.h - xash supported formats
//=======================================================================
#ifndef REF_FORMAT_H
#define REF_FORMAT_H

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// header
#define IDPACKV1HEADER	(('K'<<24)+('C'<<16)+('A'<<8)+'P')	// little-endian "PACK"
#define IDPACKV2HEADER	(('2'<<24)+('K'<<16)+('A'<<8)+'P')	// little-endian "PAK2"
#define IDPACKV3HEADER	(('\4'<<24)+('\3'<<16)+('K'<<8)+'P')	// little-endian "PK\3\4"
#define IDPK3CDRHEADER	(('\2'<<24)+('\1'<<16)+('K'<<8)+'P')	// little-endian "PK\1\2"
#define IDPK3ENDHEADER	(('\6'<<24)+('\5'<<16)+('K'<<8)+'P')	// little-endian "PK\5\6"

#define MAX_FILES_IN_PACK		65536 // pack\pak2

typedef struct
{
	int		ident;
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char		name[56];		// total 64 bytes
	int		filepos;
	int		filelen;
} dpackfile_t;

typedef struct
{
	char		name[116];	// total 128 bytes
	int		filepos;
	int		filelen;
	uint		attribs;		// file attributes
} dpak2file_t;

typedef struct
{
	int		ident;
	word		disknum;
	word		cdir_disknum;	// number of the disk with the start of the central directory
	word		localentries;	// number of entries in the central directory on this disk
	word		nbentries;	// total number of entries in the central directory on this disk
	uint		cdir_size;	// size of the central directory
	uint		cdir_offset;	// with respect to the starting disk number
	word		comment_size;
} dpak3file_t;

/*
========================================================================
ROQ FILES

The .roq file are vector-compressed movies
========================================================================
*/
#define IDQMOVIEHEADER	0x1084	// little-endian "„"

typedef struct roq_s
{
	word	ident;
	short	flags;
	short	flags2;
	word	fps;
} roq_t;

/*
==============================================================================

STUDIO MODELS

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

// header
#define STUDIO_VERSION	10
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDST"
#define IDSEQGRPHEADER	(('Q'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSQ"

// studio limits
#define MAXSTUDIOTRIANGLES		32768	// max triangles per model
#define MAXSTUDIOVERTS		4096	// max vertices per submodel
#define MAXSTUDIOSEQUENCES		256	// total animation sequences
#define MAXSTUDIOSKINS		128	// total textures
#define MAXSTUDIOSRCBONES		512	// bones allowed at source movement
#define MAXSTUDIOBONES		128	// total bones actually used
#define MAXSTUDIOMODELS		32	// sub-models per model
#define MAXSTUDIOBODYPARTS		32	// body parts per submodel
#define MAXSTUDIOGROUPS		16	// sequence groups (e.g. barney01.mdl, barney02.mdl, e.t.c)
#define MAXSTUDIOANIMATIONS		512	// max frames per sequence
#define MAXSTUDIOMESHES		256	// max textures per model
#define MAXSTUDIOEVENTS		1024	// events per model
#define MAXSTUDIOPIVOTS		256	// pivot points
#define MAXSTUDIOBLENDS		8	// max anim blends
#define MAXSTUDIOCONTROLLERS		32	// max controllers per model
#define MAXSTUDIOATTACHMENTS		32	// max attachments per model

// model global flags
#define STUDIO_STATIC		0x0001	// model without anims
#define STUDIO_RAGDOLL		0x0002	// ragdoll animation pose

// lighting & rendermode options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT		0x0004
#define STUDIO_NF_RESERVED		0x0008	// reserved
#define STUDIO_NF_BLENDED		0x0010	// rendering as semiblended
#define STUDIO_NF_ADDITIVE		0x0020	// rendering with additive mode
#define STUDIO_NF_TRANSPARENT		0x0040	// use texture with alpha channel

// motion flags
#define STUDIO_X			0x0001
#define STUDIO_Y			0x0002	
#define STUDIO_Z			0x0004
#define STUDIO_XR			0x0008
#define STUDIO_YR			0x0010
#define STUDIO_ZR			0x0020
#define STUDIO_LX			0x0040
#define STUDIO_LY			0x0080
#define STUDIO_LZ			0x0100
#define STUDIO_AX			0x0200
#define STUDIO_AY			0x0400
#define STUDIO_AZ			0x0800
#define STUDIO_AXR			0x1000
#define STUDIO_AYR			0x2000
#define STUDIO_AZR			0x4000
#define STUDIO_TYPES		0x7FFF
#define STUDIO_RLOOP		0x8000	// controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING		0x0001

// render flags
#define STUDIO_RENDER		0x0001
#define STUDIO_EVENTS		0x0002

// bone flags
#define STUDIO_HAS_NORMALS		0x0001
#define STUDIO_HAS_VERTICES		0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME		0x0008	// if any of the textures have chrome on them

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

typedef struct
{
	int		ident;
	int		version;

	char		name[64];
	int		length;

	vec3_t		eyeposition;	// ideal eye position
	vec3_t		min;		// ideal movement hull size
	vec3_t		max;			

	vec3_t		bbmin;		// clipping bounding box
	vec3_t		bbmax;		

	int		flags;

	int		numbones;		// bones
	int		boneindex;

	int		numbonecontrollers;	// bone controllers
	int		bonecontrollerindex;

	int		numhitboxes;	// complex bounding boxes
	int		hitboxindex;			
	
	int		numseq;		// animation sequences
	int		seqindex;

	int		numseqgroups;	// demand loaded sequences
	int		seqgroupindex;

	int		numtextures;	// raw textures
	int		textureindex;
	int		texturedataindex;

	int		numskinref;	// replaceable textures
	int		numskinfamilies;
	int		skinindex;

	int		numbodyparts;		
	int		bodypartindex;

	int		numattachments;	// queryable attachable points
	int		attachmentindex;

	int		soundtable;
	int		soundindex;
	int		soundgroups;
	int		soundgroupindex;

	int		numtransitions;	// animation node to animation node transition graph
	int		transitionindex;
} studiohdr_t;

// header for demand loaded sequence group data
typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;
} studioseqhdr_t;

// bones
typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		flags;		// ??
	int		bonecontroller[6];	// bone controller index, -1 == none
	float		value[6];		// default DoF values
	float		scale[6];		// scale for delta DoF values
} mstudiobone_t;

// bone controllers
typedef struct 
{
	int		bone;		// -1 == 0
	int		type;		// X, Y, Z, XR, YR, ZR, M
	float		start;
	float		end;
	int		rest;		// byte index value at rest
	int		index;		// 0-3 user set controller, 4 mouth
} mstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int		bone;
	int		group;		// intersection group
	vec3_t		bbmin;		// bounding box
	vec3_t		bbmax;		
} mstudiobbox_t;

typedef struct cache_user_s
{
	void *data;
} cache_user_t;

// demand loaded sequence groups
typedef struct
{
	char		label[32];	// textual name
	char		name[64];		// file name
	void*		cache;		// cache index pointer
	int		data;		// hack for group 0
} mstudioseqgroup_t;

// sequence descriptions
typedef struct
{
	char		label[32];	// sequence label (name)

	float		fps;		// frames per second	
	int		flags;		// looping/non-looping flags

	int		activity;
	int		actweight;

	int		numevents;
	int		eventindex;

	int		numframes;	// number of frames per sequence

	int		numpivots;	// number of foot pivots
	int		pivotindex;

	int		motiontype;	
	int		motionbone;
	vec3_t		linearmovement;
	int		automoveposindex;
	int		automoveangleindex;

	vec3_t		bbmin;		// per sequence bounding box
	vec3_t		bbmax;		

	int		numblends;
	int		animindex;	// mstudioanim_t pointer relative to start of sequence group data
					// [blend][bone][X, Y, Z, XR, YR, ZR]

	int		blendtype[2];	// X, Y, Z, XR, YR, ZR
	float		blendstart[2];	// starting value
	float		blendend[2];	// ending value
	int		blendparent;

	int		seqgroup;		// sequence group for demand loading

	int		entrynode;	// transition node at entry
	int		exitnode;		// transition node at exit
	int		nodeflags;	// transition rules
	
	int		nextseq;		// auto advancing sequences
} mstudioseqdesc_t;

// events
typedef struct 
{
	int 		frame;
	int		event;
	int		type;
	char		options[64];
} mstudioevent_t;


// pivots
typedef struct 
{
	vec3_t		org;		// pivot point
	int		start;
	int		end;
} mstudiopivot_t;

// attachment
typedef struct 
{
	char		name[32];
	int		type;
	int		bone;
	vec3_t		org;		// attachment point
	vec3_t		vectors[3];
} mstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} mstudioanim_t;

// animation frames
typedef union 
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} mstudioanimvalue_t;


// body part index
typedef struct
{
	char		name[64];
	int		nummodels;
	int		base;
	int		modelindex;	// index into models array
} mstudiobodyparts_t;


// skin info
typedef struct
{
	char		name[64];
	int		flags;
	int		width;
	int		height;
	int		index;
} mstudiotexture_t;

// skin families
// short	index[skinfamilies][skinref]		// skingroup info

// studio models
typedef struct
{
	char		name[64];

	int		type;
	float		boundingradius;

	int		nummesh;
	int		meshindex;

	int		numverts;		// number of unique vertices
	int		vertinfoindex;	// vertex bone info
	int		vertindex;	// vertex vec3_t
	int		numnorms;		// number of unique surface normals
	int		norminfoindex;	// normal bone info
	int		normindex;	// normal vec3_t

	int		numgroups;	// deformation groups
	int		groupindex;
} mstudiomodel_t;

// meshes
typedef struct 
{
	int		numtris;
	int		triindex;
	int		skinref;
	int		numnorms;		// per mesh normals
	int		normindex;	// normal vec3_t
} mstudiomesh_t;

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

/*
==============================================================================

BRUSH MODELS
==============================================================================
*/

// header
#define BSPMOD_VERSION	39
#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"

// 32 bit limits
#define MAX_MAP_SIZE		0x40000	// -/+ 131072
#define MAX_KEY			128
#define MAX_VALUE			512
#define MAX_MAP_AREAS		0x100	// don't increase this
#define MAX_MAP_MODELS		0x2000	// mesh models and sprites too
#define MAX_MAP_AREAPORTALS		0x400
#define MAX_MAP_ENTITIES		0x2000
#define MAX_MAP_TEXINFO		0x2000
#define MAX_MAP_BRUSHES		0x8000
#define MAX_MAP_PLANES		0x20000
#define MAX_MAP_NODES		0x20000
#define MAX_MAP_BRUSHSIDES		0x20000
#define MAX_MAP_LEAFS		0x20000
#define MAX_MAP_VERTS		0x80000
#define MAX_MAP_FACES		0x20000
#define MAX_MAP_LEAFFACES		0x20000
#define MAX_MAP_LEAFBRUSHES		0x40000
#define MAX_MAP_PORTALS		0x20000
#define MAX_MAP_EDGES		0x40000
#define MAX_MAP_SURFEDGES		0x40000
#define MAX_MAP_ENTSTRING		0x80000
#define MAX_MAP_LIGHTING		0x800000
#define MAX_MAP_VISIBILITY		0x800000
#define MAX_MAP_COLLISION		0x800000
#define MAX_MAP_STRINGDATA		0x40000
#define MAX_MAP_NUMSTRINGS		0x10000

// lump offset
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_LEAFS			2
#define LUMP_LEAFFACES		3
#define LUMP_LEAFBRUSHES		4
#define LUMP_NODES			5
#define LUMP_VERTEXES		6
#define LUMP_EDGES			7
#define LUMP_SURFEDGES		8
#define LUMP_SURFDESC		9
#define LUMP_FACES			10
#define LUMP_MODELS			11
#define LUMP_BRUSHES		12
#define LUMP_BRUSHSIDES		13
#define LUMP_VISIBILITY		14
#define LUMP_LIGHTING		15
#define LUMP_COLLISION		16	// newton collision tree (worldmodel coords already convert to meters)
#define LUMP_BVHSTATIC		17	// bullet collision tree (currently not used)
#define LUMP_SVPROGS		18	// private server.dat for current map
#define LUMP_WAYPOINTS		19	// AI navigate tree (like .aas file for quake3)		
#define LUMP_STRINGDATA		20	// string array
#define LUMP_STRINGTABLE		21	// string table id's

// get rid of this
#define LUMP_AREAS			22
#define LUMP_AREAPORTALS		23

#define LUMP_TOTALCOUNT		32	// max lumps


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS			0
#define DVIS_PHS			1

//other limits
#define MAXLIGHTMAPS		4

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[LUMP_TOTALCOUNT];
} dheader_t;

typedef struct
{
	float	mins[3];
	float	maxs[3];
	int	headnode;
	int	firstface;	// submodels just draw faces 
	int	numfaces;		// without walking the bsp tree
	int	firstbrush;	// physics stuff
	int	numbrushes;
} dmodel_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	int	mins[3];		// for frustom culling
	int	maxs[3];
	int	firstface;
	int	numfaces;		// counting both sides
} dnode_t;

typedef struct dsurfdesc_s
{
	float	vecs[2][4];	// [s/t][xyz offset] texture s\t
	int	size[2];		// valid size for current s\t coords (used for replace texture)
	int	texid;		// string table texture id number
	int	animid;		// string table animchain id number 
	int	flags;		// surface flags
	int	value;		// used by qrad, not engine
} dsurfdesc_t;

typedef struct
{
	int	v[2];		// vertex numbers
} dedge_t;

typedef struct
{
	int	planenum;
	int	firstedge;
	int	numedges;	
	int	desc;

	// lighting info
	byte	styles[MAXLIGHTMAPS];
	int	lightofs;		// start of [numstyles*surfsize] samples

	// get rid of this
	short	side;
} dface_t;

typedef struct
{
	int	contents;		// or of all brushes (not needed?)
	int	cluster;
	int	area;
	int	mins[3];		// for frustum culling
	int	maxs[3];
	int	firstleafface;
	int	numleaffaces;
	int	firstleafbrush;
	int	numleafbrushes;
} dleaf_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	surfdesc;		// surface description (s/t coords, flags, etc)
} dbrushside_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	contents;
} dbrush_t;

typedef struct
{
	int	numclusters;
	int	bitofs[8][2];	// bitofs[numclusters][2]
} dvis_t;

typedef struct
{
	int	portalnum;
	int	otherarea;
} dareaportal_t;

typedef struct
{
	int	numareaportals;
	int	firstareaportal;
} darea_t;

/*
==============================================================================

MAP CONTENTS & SURFACES DESCRIPTION
==============================================================================
*/
#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_NONE		0 	// just a mask for source tabulation
#define CONTENTS_SOLID		1 	// an eye is never valid in a solid
#define CONTENTS_WINDOW		2 	// translucent, but not watery
#define CONTENTS_AUX		4
#define CONTENTS_LAVA		8
#define CONTENTS_SLIME		16
#define CONTENTS_WATER		32
#define CONTENTS_MIST		64
#define LAST_VISIBLE_CONTENTS		64
#define CONTENTS_MUD		128	// not a "real" content property - used only for watertype

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_FOG		0x4000	//future expansion
#define CONTENTS_AREAPORTAL		0x8000
#define CONTENTS_PLAYERCLIP		0x10000
#define CONTENTS_MONSTERCLIP		0x20000
#define CONTENTS_CLIP		CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP //both type clip
#define CONTENTS_CURRENT_0		0x40000	// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_90		0x80000
#define CONTENTS_CURRENT_180		0x100000
#define CONTENTS_CURRENT_270		0x200000
#define CONTENTS_CURRENT_UP		0x400000
#define CONTENTS_CURRENT_DOWN		0x800000
#define CONTENTS_ORIGIN		0x1000000	// removed before bsping an entity
#define CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define CONTENTS_DEADMONSTER		0x4000000
#define CONTENTS_DETAIL		0x8000000	// brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT		0x10000000// auto set if any surface has trans
#define CONTENTS_LADDER		0x20000000

//surfaces
#define SURF_NONE			0 	// just a mask for source tabulation
#define SURF_LIGHT			0x1	// value will hold the light strength
#define SURF_SLICK			0x2	// effects game physics
#define SURF_SKY			0x4	// don't draw, but add to skybox
#define SURF_WARP			0x8	// turbulent water warp
#define SURF_TRANS			0x10	// grates
#define SURF_BLEND			0x20	// windows
#define SURF_FLOWING		0x40	// scroll towards angle
#define SURF_NODRAW			0x80	// don't bother referencing the texture
#define SURF_HINT			0x100	// make a primary bsp splitter
#define SURF_SKIP			0x200	// completely ignore, allowing non-closed brushes
#define SURF_NULL			0x400	// remove face after compile
#define SURF_MIRROR			0x800	// remove face after compile
#define SURF_METAL			0x00000400// metal floor
#define SURF_DIRT			0x00000800// dirt, sand, rock
#define SURF_VENT			0x00001000// ventillation duct
#define SURF_GRATE			0x00002000// metal grating
#define SURF_TILE			0x00004000// floor tiles
#define SURF_GRASS      		0x00008000// grass
#define SURF_SNOW       		0x00010000// snow
#define SURF_CARPET     		0x00020000// carpet
#define SURF_FORCE      		0x00040000// forcefield
#define SURF_GRAVEL     		0x00080000// gravel
#define SURF_ICE			0x00100000// ice
#define SURF_STANDARD   		0x00200000// normal footsteps
#define SURF_STEPMASK		0x000FFC00

/*
==============================================================================

VIRTUAL MACHINE

a internal virtual machine like as QuakeC, but it has more extensions
==============================================================================
*/

// header
#define QPROGS_VERSION	6	// Quake1 progs version
#define FPROGS_VERSION	7	// Fte progs version
#define VPROGS_VERSION	8	// xash progs version
#define LNNUMS_VERSION	1	// line numbers version

#define VPROGSHEADER16	(('6'<<24)+('1'<<16)+('D'<<8)+'I') // little-endian "ID16"
#define VPROGSHEADER32	(('2'<<24)+('3'<<16)+('D'<<8)+'I') // little-endian "ID32"
#define LINENUMSHEADER	(('F'<<24)+('O'<<16)+('N'<<8)+'L') // little-endian "LNOF"

// global ofssets
#define OFS_NULL		0
#define OFS_RETURN		1
#define OFS_PARM0		4
#define OFS_PARM1		7
#define OFS_PARM2		10
#define OFS_PARM3		13
#define OFS_PARM4		16
#define OFS_PARM5		19
#define OFS_PARM6		22
#define OFS_PARM7		25
#define RESERVED_OFS	28

// misc flags
#define DEF_SHARED		(1<<14)
#define DEF_SAVEGLOBAL	(1<<15)

// compression block flags
#define COMP_STATEMENTS	1
#define COMP_DEFS		2
#define COMP_FIELDS		4
#define COMP_FUNCTIONS	8
#define COMP_STRINGS	16
#define COMP_GLOBALS	32
#define COMP_LINENUMS	64
#define COMP_TYPES		128
#define COMP_SOURCE		256
#define MAX_PARMS		8

// 16-bit mode
#define dstatement_t	dstatement16_t
#define ddef_t		ddef16_t		// these should be the same except the string type

typedef enum 
{
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_function,
	ev_pointer,
	ev_integer,
	ev_variant,
	ev_struct,
	ev_union,
} etype_t;

enum op_state
{
	OP_DONE,		// 0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,	// (vec3_t) * (float)
	OP_MUL_VF,          // (float) * (vec3_t)
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,		// 10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,		// = (float) <= (float);
	OP_GE,		// = (float) >= (float);
	OP_LT,		// = (float) <  (float);
	OP_GT,		// = (float) >  (float);

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	// 30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	// 40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,		// 50
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		// 60
	OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,	// = (float) & (float); // of cource converting into integer in real code
	OP_BITOR,

	// version 7 started
	OP_MULSTORE_F,	// f *= f
	OP_MULSTORE_V,	// v *= f
	OP_MULSTOREP_F,	// e.f *= f
	OP_MULSTOREP_V,	// e.v *= f

	OP_DIVSTORE_F,	// f /= f
	OP_DIVSTOREP_F,	// e.f /= f

	OP_ADDSTORE_F,	// f += f
	OP_ADDSTORE_V,	// v += v
	OP_ADDSTOREP_F,	// e.f += f
	OP_ADDSTOREP_V,	// e.v += v

	OP_SUBSTORE_F,	// f -= f
	OP_SUBSTORE_V,	// v -= v
	OP_SUBSTOREP_F,	// e.f -= f
	OP_SUBSTOREP_V,	// e.v -= v

	OP_FETCH_GBL_F,	// 80
	OP_FETCH_GBL_V,
	OP_FETCH_GBL_S,
	OP_FETCH_GBL_E,
	OP_FETCH_G_FNC,

	OP_CSTATE,
	OP_CWSTATE,

	OP_THINKTIME,

	OP_BITSET,	// b  (+) a
	OP_BITSETP,	// .b (+) a
	OP_BITCLR,	// b  (-) a
	OP_BITCLRP,	// .b (-) a

	OP_RAND0,
	OP_RAND1,
	OP_RAND2,
	OP_RANDV0,
	OP_RANDV1,
	OP_RANDV2,

	OP_SWITCH_F,	// switches
	OP_SWITCH_V,	// 100
	OP_SWITCH_S,
	OP_SWITCH_E,
	OP_SWITCH_FNC,

	OP_CASE,
	OP_CASERANGE,

	OP_STORE_I,
	OP_STORE_IF,
	OP_STORE_FI,
	
	OP_ADD_I,
	OP_ADD_FI,	// 110
	OP_ADD_IF,
  
	OP_SUB_I,
	OP_SUB_FI,
	OP_SUB_IF,

	OP_CONV_ITOF,
	OP_CONV_FTOI,
	OP_CP_ITOF,
	OP_CP_FTOI,
	OP_LOAD_I,
	OP_STOREP_I,	// 120
	OP_STOREP_IF,
	OP_STOREP_FI,

	OP_BITAND_I,
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOTS,
	OP_IFS,		// 130

	OP_NOT_I,

	OP_DIV_VF,

	OP_POWER_I,
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBAL_ADD,
	OP_POINTER_ADD,	// pointer to 32 bit (remember to *3 for vectors)

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,	// 140
	OP_LOADA_ENT,
	OP_LOADA_FLD,		
	OP_LOADA_FNC,
	OP_LOADA_I,

	OP_STORE_P,
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,	// 150
	OP_LOADP_FLD,
	OP_LOADP_FNC,
	OP_LOADP_I,

	OP_LE_I,            // (int)c = (int)a <= (int)b;
	OP_GE_I,		// (int)c = (int)a >= (int)b;
	OP_LT_I,		// (int)c = (int)a <  (int)b;
	OP_GT_I,		// (int)c = (int)a >  (int)b;

	OP_LE_IF,           // (float)c = (int)a <= (float)b;
	OP_GE_IF,		// (float)c = (int)a >= (float)b;
	OP_LT_IF,		// (float)c = (int)a <  (float)b;
	OP_GT_IF,		// (float)c = (int)a >  (float)b;

	OP_LE_FI,		// (float)c = (float)a <= (int)b;
	OP_GE_FI,		// (float)c = (float)a >= (int)b;
	OP_LT_FI,		// (float)c = (float)a <  (int)b;
	OP_GT_FI,		// (float)c = (float)a >  (int)b;

	OP_EQ_IF,
	OP_EQ_FI,

	OP_ADD_SF,	// (char*)c = (char*)a + (float)b
	OP_SUB_S,		// (float)c = (char*)a - (char*)b
	OP_STOREP_C,	// (float)c = *(char*)b = (float)a
	OP_LOADP_C,	// (float)c = *(char*) // 170

	OP_MUL_IF,
	OP_MUL_FI,
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,	// 180
	OP_BITOR_FI,
	OP_AND_I,
	OP_OR_I,
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

	OP_GSTOREP_I,	// 190
	OP_GSTOREP_F,		
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,	// integers
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,	// pointers
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,	// 200
	OP_GLOAD_ENT,
	OP_GLOAD_S,
	OP_GLOAD_FNC,
	OP_GLOAD_V,
	OP_BOUNDCHECK,

	OP_STOREP_P,	// back to ones that we do use.	
	OP_PUSH,
	OP_POP,

	// version 8 started
	OP_NUMOPS,
};

typedef struct statement16_s
{
	word		op;
	short		a,b,c;

} dstatement16_t;

typedef struct statement32_s
{
	dword		op;
	long		a,b,c;

} dstatement32_t;

typedef struct ddef16_s
{
	word		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	word		ofs;
	string_t		s_name;
} ddef16_t;

typedef struct ddef32_s
{
	dword		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	dword		ofs;
	string_t		s_name;
} ddef32_t;

typedef struct fdef_s
{
	uint		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	uint		ofs;
	uint		progsofs;		// used at loading time, so maching field offsets (unions/members) 
					// are positioned at the same runtime offset.
	char		*name;
} fdef_t;

typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals
	int		profile;		// runtime
	string_t		s_name;
	string_t		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} dfunction_t;

typedef struct
{
	int		version;		// version number
	int		crc;		// check of header file
	
	uint		ofs_statements;	// comp 1
	uint		numstatements;	// statement 0 is an error
	uint		ofs_globaldefs;	// comp 2
	uint		numglobaldefs;
	uint		ofs_fielddefs;	// comp 4
	uint		numfielddefs;
	uint		ofs_functions;	// comp 8
	uint		numfunctions;	// function 0 is an empty
	uint		ofs_strings;	// comp 16
	uint		numstrings;	// first string is a null string
	uint		ofs_globals;	// comp 32
	uint		numglobals;
	uint		entityfields;

	// version 7 extensions
	uint		ofsfiles;		// source files always compressed
	uint		ofslinenums;	// numstatements big // comp 64
	uint		ofsbodylessfuncs;	// no comp
	uint		numbodylessfuncs;
	uint		ofs_types;	// comp 128
	uint		numtypes;
	uint		blockscompressed;	// who blocks are compressed (COMP flags)

	int		ident;		// version 7 id

} dprograms_t;

typedef struct dlno_s
{
	int	header;
	int	version;

	uint	numglobaldefs;
	uint	numglobals;
	uint	numfielddefs;
	uint	numstatements;
} dlno_t;

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit
	double		profile;		// runtime
	double		builtinsprofile;	// cost of builtin functions called by this function
	double		callcount;	// times the functions has been called since the last profile call

	int		s_name;
	int		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} mfunction_t;

typedef struct
{
	char		filename[128];
	int		size;
	int		compsize;
	int		compmethod;
	int		ofs;
} includeddatafile_t;

typedef struct type_s
{
	etype_t		type;

	struct type_s	*parentclass;	// type_entity...
	struct type_s	*next;
	struct type_s	*aux_type;	// return type or field type
	struct type_s	*param;

	int		num_parms;	// -1 = variable args
	uint		ofs;		// inside a structure.
	uint		size;
	char		*name;
} type_t;

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
#define SURF_PLANEBACK		2
#define SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x10
#define SURF_DRAWBACKGROUND		0x40
#define SURF_UNDERWATER		0x80

typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;

typedef struct cmesh_s
{
	vec3_t	*verts;
	uint	numverts;
} cmesh_t;

typedef struct cmodel_s
{
	int	modidx;		// edict index
	char	name[64];		// model name
	byte	*mempool;		// personal mempool

	vec3_t	mins, maxs;	// boundbox
	int	headnode;		// bsp info

	int	numframes;	// sprite framecount
	cmesh_t	physmesh[MAXSTUDIOMODELS]; // max bodies

	void	*extradata;	// get rid of this
} cmodel_t;

typedef struct csurface_s
{
	char	name[16];
	int	flags;
	int	value;

	// physics stuff
	int	firstedge;	// look up in model->surfedges[], negative numbers
	int	numedges;		// are backwards edges
} csurface_t;

typedef struct mapsurface_s
{
	csurface_t c;
	char	rname[32];
} mapsurface_t;

typedef struct trace_s
{
	bool		allsolid;		// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	bool		startstuck;	// trace started from solid entity
	float		fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	csurface_t	*surface;		// surface hit
	int		contents;		// contents on other side of surface hit
	int		hitgroup;		// hit a studiomodel hitgroup #
	int		flags;		// misc trace flags
	edict_t		*ent;		// not set by CM_*() functions
} trace_t;

/*
==============================================================================

SAVE FILE
==============================================================================
*/
#define SAVE_VERSION	3
#define IDSAVEHEADER	(('E'<<24)+('V'<<16)+('A'<<8)+'S') // little-endian "SAVE"

#define LUMP_COMMENTS	0 // map comments
#define LUMP_CFGSTRING	1 // client info strings
#define LUMP_AREASTATE	2 // area portals state
#define LUMP_GAMESTATE	3 // progs global state (deflated)
#define LUMP_MAPNAME	4 // map name
#define LUMP_GAMECVARS	5 // contain game comment and all cvar state
#define LUMP_GAMEENTS	6 // ents state (deflated)
#define LUMP_SNAPSHOT	7 // tga image snapshot (128x128)
#define SAVE_NUMLUMPS	8 // header size

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[SAVE_NUMLUMPS];
} dsavehdr_t;

typedef struct
{
	char	name[MAX_QPATH];
	char	value[MAX_QPATH];
} dsavecvar_t;

/*
========================================================================

.PCX image format	(ZSoft Paintbrush)

========================================================================
*/
typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin,ymin,xmax,ymax;
	word	hres,vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
} pcx_t;

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
========================================================================

.LMP image format	(Quake1 gfx lumps)

========================================================================
*/
typedef struct flat_s
{
	short	width;
	short	height;
	short	desc[2];		// probably not used
} flat_t;

/*
========================================================================

.LMP image format	(Quake1 gfx lumps)

========================================================================
*/
typedef struct lmp_s
{
	uint	width;
	uint	height;
} lmp_t;

/*
========================================================================

.MIP image format	(Quake1 textures)

========================================================================
*/
typedef struct mip_s
{
	char	name[16];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
} mip_t;

/*
========================================================================

.WAD archive format	(WhereAllData - WAD)

========================================================================
*/
#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad
#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD3" half-life wads
#define IDWAD4HEADER	(('4'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD4" Xash3D wad type
#define WAD3_NAMELEN	16

#define	CMP_NONE		0	// compression none
#define	CMP_LZSS		1	// RLE compression ?
#define	CMP_ZLIB		2	// zip-archive compression

#define	TYPE_NONE		0	// blank lump
#define	TYPE_SKIN		62	// virtual lump type (not a visible charcster)
#define	TYPE_FLAT		63	// virtual lump type (not a visible charcster)
#define	TYPE_QPAL		64	// quake palette
#define	TYPE_QTEX		65	// probably was never used
#define	TYPE_QPIC		66	// quake1 and hl pic (lmp_t)
#define	TYPE_MIPTEX2	67	// half-life (mip_t) previous TYP_SOUND but never used in quake1
#define	TYPE_MIPTEX	68	// quake1 (mip_t)
#define	TYPE_RAW		69	// raw data
#define	TYPE_QFONT	70	// half-life font	(qfont_t)
#define	TYPE_MIPDDS	71	// Xash3D dds texture
#define	TYPE_MIPTGA	72	// Xash3D tga texture
#define	TYPE_VPROGS	73	// Xash3D QC compiled progs
#define	TYPE_SCRIPT	74	// txt script file (e.g. shader)
#define	TYPE_STUDIO	75	// studio models
#define	TYPE_SPRITE	76	// normal sprite
#define	TYPE_SOUND	77	// wav data
#define	TYPE_ENTFILE	78	// map entities description

#define	QCHAR_WIDTH	16
#define	QFONT_WIDTH	16	// valve fonts used contant sizes	
#define	QFONT_HEIGHT        ((128 - 32) / 16)

typedef struct
{
	int		ident;		// should be IWAD or WAD2 or WAD3
	int		numlumps;
	int		infotableofs;
} dwadinfo_t;

// doom1 and doom2 lump header
typedef struct
{
    int			filepos;
    int			size;
    char			name[8];
} dlumpfile_t;

// quake1 and half-life lump header
typedef struct
{
	int		filepos;
	int		disksize;
	int		size;		// uncompressed
	char		type;
	char		compression;	// probably not used
	char		pad1;
	char		pad2;
	char		name[16];	// must be null terminated
} dlumpinfo_t;

typedef struct
{
	short startoffset;
	short charwidth;
} charset;

typedef struct
{
	int 		width, height;
	int		rowcount;
	int		rowheight;
	charset		fontinfo[256];
	byte 		data[4];		// variable sized
} qfont_t;

/*
========================================================================

.TGA image format	(Truevision Targa)

========================================================================
*/
typedef struct tga_s
{
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;
/*
========================================================================

.DDS image format

========================================================================
*/
#define DDSHEADER	((' '<<24)+('S'<<16)+('D'<<8)+'D') // little-endian "DDS "

//other four-cc types
#define TYPE_DXT1	(('1'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT1"
#define TYPE_DXT2	(('2'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT2"
#define TYPE_DXT3	(('3'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT3"
#define TYPE_DXT4	(('4'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT4"
#define TYPE_DXT5	(('5'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT5"

#define TYPE_ATI1	(('1'<<24)+('I'<<16)+('T'<<8)+'A') // little-endian "ATI1"
#define TYPE_ATI2	(('2'<<24)+('I'<<16)+('T'<<8)+'A') // little-endian "ATI2"


#define TYPE_RXGB	(('B'<<24)+('G'<<16)+('X'<<8)+'R') // little-endian "RXGB" doom3 normalmaps
#define TYPE_$	(('\0'<<24)+('\0'<<16)+('\0'<<8)+'$') // little-endian "$"

#define DDS_CAPS				0x00000001L
#define DDS_HEIGHT				0x00000002L
#define DDS_WIDTH				0x00000004L

#define DDS_RGB				0x00000040L
#define DDS_PIXELFORMAT			0x00001000L
#define DDS_LUMINANCE			0x00020000L

#define DDS_ALPHAPIXELS			0x00000001L
#define DDS_ALPHA				0x00000002L
#define DDS_FOURCC				0x00000004L
#define DDS_PITCH				0x00000008L
#define DDS_COMPLEX				0x00000008L
#define DDS_CUBEMAP				0x00000200L
#define DDS_TEXTURE				0x00001000L
#define DDS_MIPMAPCOUNT			0x00020000L
#define DDS_LINEARSIZE			0x00080000L
#define DDS_VOLUME				0x00200000L
#define DDS_MIPMAP				0x00400000L
#define DDS_DEPTH				0x00800000L

#define DDS_CUBEMAP_POSITIVEX			0x00000400L
#define DDS_CUBEMAP_NEGATIVEX			0x00000800L
#define DDS_CUBEMAP_POSITIVEY			0x00001000L
#define DDS_CUBEMAP_NEGATIVEY			0x00002000L
#define DDS_CUBEMAP_POSITIVEZ			0x00004000L
#define DDS_CUBEMAP_NEGATIVEZ			0x00008000L

typedef struct dds_pf_s
{
	uint	dwSize;
	uint	dwFlags;
	uint	dwFourCC;
	uint	dwRGBBitCount;
	uint	dwRBitMask;
	uint	dwGBitMask;
	uint	dwBBitMask;
	uint	dwABitMask;
} dds_pixf_t;

//  DDCAPS2
typedef struct dds_caps_s
{
	uint	dwCaps1;
	uint	dwCaps2;
	uint	dwCaps3;
	uint	dwCaps4;
} dds_caps_t;

typedef struct
{
	uint		dwIdent;		// must matched with DDSHEADER
	uint		dwSize;
	uint		dwFlags;		// determines what fields are valid
	uint		dwHeight;
	uint		dwWidth;
	uint		dwLinearSize;	// Formless late-allocated optimized surface size
	uint		dwDepth;		// depth if a volume texture
	uint		dwMipMapCount;	// number of mip-map levels requested
	uint		dwAlphaBitDepth;	// depth of alpha buffer requested
	float		fReflectivity[3];	// average reflectivity value
	float		fBumpScale;	// bumpmapping scale factor
	uint		dwReserved1[6];	// reserved for future expansions
	dds_pixf_t	dsPixelFormat;
	dds_caps_t	dsCaps;
	uint		dwTextureStage;
} dds_t;

/*
========================================================================

.JPG image format

========================================================================
*/
typedef struct huffman_table_s
{
	// Huffman coding tables
	byte	bits[16];
	byte	hval[256];
	byte	size[256];
	word	code[256];

} huffman_table_t;

typedef struct jpg_s
{
	// not a real header
	file_t	*file;		// file
	byte	*buffer;		// jpg buffer
	
	int	width;		// width image
	int	height;		// height image
	byte	*data;		// image
	int	data_precision;	// bit per component
	int	num_components;	// number component
	int	restart_interval;	// restart interval
	bool	progressive_mode;	// progressive format

	struct
	{
		int     id;	// identifier
		int     h;	// horizontal sampling factor
		int     v;	// vertical sampling factor
		int     t;	// quantization table selector
		int     td;	// DC table selector
		int     ta;	// AC table selector
	} component_info[3];	// RGB (alpha not supported)
    
	huffman_table_t hac[4];	// AC table
	huffman_table_t hdc[4];	// DC table

	int	qtable[4][64];	// quantization table

	struct
	{
		int     ss,se;	// progressive jpeg spectral selection
		int     ah,al;	// progressive jpeg successive approx
	} scan;

	int	dc[3];
	int	curbit;
	byte	curbyte;

} jpg_t;

/*
========================================================================

internal image format

typically expanded to rgba buffer
========================================================================
*/
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

typedef struct bpc_desc_s
{
	int	format;	// pixelformat
	char	name[8];	// used for debug
	uint	glmask;	// RGBA mask
	uint	gltype;	// open gl datatype
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
	int	bpc;	// sizebytes (byte, short, float)
	int	block;	// blocksize < 0 needs alternate calc
} bpc_desc_t;

static const bpc_desc_t PFDesc[] =
{
{PF_UNKNOWN,	"raw",	0x1908,	0x1401, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	0x1908,	0x1401, 3,  1,  0 },// expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	0x1908,	0x1401, 4,  1,  0 },
{PF_RGBA_32,	"RGBA 32",0x1908,	0x1401, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",0x1908,	0x1401, 4,  1, -4 },
{PF_RGB_24,	"RGB 24",	0x1908,	0x1401, 3,  1, -3 },
{PF_RGB_24_FLIP,	"RGB 24",	0x1908,	0x1401, 3,  1, -3 },
{PF_DXT1,		"DXT1",	0x1908,	0x1401, 4,  1,  8 },
{PF_DXT2,		"DXT2",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT3,		"DXT3",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT4,		"DXT4",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT5,		"DXT5",	0x1908,	0x1401, 4,  1, 16 },
{PF_ATI1N,	"ATI1N",	0x1908,	0x1401, 1,  1,  8 },
{PF_ATI2N,	"3DC",	0x1908,	0x1401, 3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	0x1909,	0x1401, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", 0x1909,	0x1401, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	0x190A,	0x1401, 2,  1, -2 },
{PF_RXGB,		"RXGB",	0x1908,	0x1401, 3,  1, 16 },
{PF_ABGR_64,	"ABGR 64",0x80E1,	0x1401, 4,  2, -8 },
{PF_RGBA_GN,	"system",	0x1908,	0x1401, 4,  1, -4 },
};

// description flags
#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips
#define IMAGE_CUBEMAP_FLIP	0x00000010	// it's a cubemap with flipped sides( dds pack )

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
	vec3_t	color;		// radiocity reflectivity
	float	bump_scale;	// internal bumpscale
	uint	size;		// for bounds checking
} rgbdata_t;

/*
========================================================================

internal physic data

hold linear and angular velocity, current position stored too
========================================================================
*/
// phys movetype
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
	MOVETYPE_PUSHABLE,
	MOVETYPE_PHYSIC,	// phys simulation
} movetype_t;

// phys collision mode
typedef enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_BOX,	// physbox
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH,	// custom convex hull
} solid_t;

typedef struct physdata_s
{
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;	// "omega" in newton
	vec3_t		mins;		// for calculate size 
	vec3_t		maxs;		// and setup offset matrix

	int		movetype;		// moving type
	int		hulltype;		// solid type

	physbody_t	*body;		// ptr to physic body
} physdata_t;

/*
========================================================================

GAMEINFO stuff

internal shared gameinfo structure (readonly for engine parts)
========================================================================
*/
typedef struct gameinfo_s
{
	// filesystem info
	char	basedir[MAX_OSPATH];
	char	gamedir[MAX_OSPATH];
	char	title[MAX_OSPATH];
          float	version;		// engine or mod version
	
	int	viewmode;
	int	gamemode;

	// shared system info
	int	cpunum;		// count of cpu's
	int64	tickcount;	// cpu frequency in 'ticks'
	float	cpufreq;		// cpu frequency in MHz
	bool	rdtsc;		// rdtsc support (profiler stuff)
	
	char	key[MAX_INFO_KEY];	// cd-key info
	char	TXcommand;	// quark command (get rid of this)
} gameinfo_t;

/*
========================================================================

internal dll's loader

two main types - native dlls and other win32 libraries will be recognized automatically
========================================================================
*/
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


/*
========================================================================

console variables

external and internal cvars struct have some differences
========================================================================
*/
#ifndef LAUNCH_DLL
typedef struct cvar_s
{
	char	*name;
	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )
	bool	modified;		// set each time the cvar is changed
} cvar_t;
#endif

#endif//REF_FORMAT_H