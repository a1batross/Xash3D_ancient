//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_edict.h - server prvm edict
//=======================================================================
#ifndef SV_EDICT_H
#define SV_EDICT_H

#define MAX_ENT_CLUSTERS			16

#define AI_FLY				(1<<0)		// monster is flying
#define AI_SWIM				(1<<1)		// swimming monster
#define AI_ONGROUND				(1<<2)		// monster is onground
#define AI_PARTIALONGROUND			(1<<3)		// monster is partially onground
#define AI_GODMODE				(1<<4)		// monster don't give damage at all
#define AI_NOTARGET				(1<<5)		// monster will no searching enemy's
#define AI_NOSTEP				(1<<6)		// Lazarus stuff
#define AI_DUCKED				(1<<7)		// monster (or player) is ducked
#define AI_JUMPING				(1<<8)		// monster (or player) is jumping
#define AI_FROZEN				(1<<9)		// stop moving, but continue thinking
#define AI_ACTOR                		(1<<10)		// disable ai for actor
#define AI_DRIVER				(1<<11)		// npc or player driving vehcicle or train
#define AI_SPECTATOR			(1<<12)		// spectator mode for clients
#define AI_WATERJUMP			(1<<13)		// npc or player take out of water

typedef struct worldsector_s
{
	int			axis;		// -1 = leaf node
	float			dist;
	struct worldsector_s	*children[2];
	sv_edict_t		*entities;
} worldsector_t;

struct gclient_s
{
	player_state_t		ps;		// communicated by server to clients
	int			ping;
};

struct sv_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool			free;
	float			freetime;	 	// sv.time when the object was freed

	// sv_private_edict_t
	worldsector_t		*worldsector;	// member of current wolrdsector
	struct sv_edict_s 		*nextedict;	// next edict in world sector
	int			clipmask;		// trace info
	int			lastcluster;	// unused if num_clusters != -1
	int			linkcount;
	int			num_clusters;	// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			areanum, areanum2;

	int			serialnumber;	// unical entity #id
	int			solid;		// see entity_state_t for details
	int			event;		// apply sv.events too
	physbody_t		*physbody;	// ptr to phys body

	// baselines
	entity_state_t		s;
	struct gclient_s		*client;		//get rid of this
};

typedef struct sv_globalvars_s
{
	int	pad[28];
	int	pev;
	int	other;
	int	world;
	float	time;
	float	frametime;
	string_t	mapname;
	string_t	startspot;
	vec3_t	spotoffset;
	float	deathmatch;
	float	coop;
	float	teamplay;
	float	serverflags;
	float	total_secrets;
	float	total_monsters;
	float	found_secrets;
	float	killed_monsters;
	vec3_t	v_forward;
	vec3_t	v_right;
	vec3_t	v_up;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_plane_dist;
	float	trace_hitgroup;
	float	trace_contents;
	int	trace_ent;
	float	trace_flags;
	func_t	main;
	func_t	StartFrame;
	func_t	EndFrame;
	func_t	PlayerPreThink;
	func_t	PlayerPostThink;
	func_t	ClientKill;
	func_t	ClientConnect;
	func_t	PutClientInServer;
	func_t	ClientDisconnect;
	func_t	ClientCommand;
} sv_globalvars_t;

typedef struct sv_entvars_s
{
	string_t	classname;
	string_t	globalname;
	float	modelindex;
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	old_origin;
	vec3_t	old_angles;
	vec3_t	velocity;
	vec3_t	avelocity;
	vec3_t	m_pmatrix[4];
	vec3_t	m_pcentre[3];
	vec3_t	torque;
	vec3_t	force;
	vec3_t	post_origin;
	vec3_t	post_angles;
	vec3_t	origin_offset;
	float	ltime;
	float	bouncetype;
	float	movetype;
	float	solid;
	vec3_t	absmin;
	vec3_t	absmax;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	int	chain;
	string_t	model;
	float	frame;
	float	sequence;
	float	renderfx;
	float	animtime;
	float	effects;
	float	skin;
	float	body;
	string_t	weaponmodel;
	float	weaponframe;
	func_t	use;
	func_t	touch;
	func_t	think;
	func_t	blocked;
	func_t	activate;
	func_t	walk;
	func_t	jump;
	func_t	duck;
	float	flags;
	float	aiflags;
	float	spawnflags;
	int	groundentity;
	float	nextthink;
	float	takedamage;
	float	health;
	float	frags;
	float	weapon;
	float	items;
	string_t	target;
	string_t	parent;
	string_t	targetname;
	int	aiment;
	int	goalentity;
	vec3_t	punchangle;
	float	deadflag;
	vec3_t	view_ofs;
	float	button0;
	float	button1;
	float	button2;
	float	impulse;
	float	fixangle;
	vec3_t	v_angle;
	float	idealpitch;
	string_t	netname;
	int	enemy;
	float	alpha;
	float	team;
	float	max_health;
	float	teleport_time;
	float	armortype;
	float	armorvalue;
	float	waterlevel;
	float	watertype;
	float	ideal_yaw;
	float	yaw_speed;
	float	dmg_take;
	float	dmg_save;
	int	dmg_inflictor;
	int	owner;
	vec3_t	movedir;
	string_t	message;
	float	sounds;
	string_t	noise;
	string_t	noise1;
	string_t	noise2;
	string_t	noise3;
	float	jumpup;
	float	jumpdn;
	int	movetarget;
	float	mass;
	float	density;
	float	gravity;
	float	dmg;
	float	dmgtime;
	float	speed;
} sv_entvars_t;

#define SV_NUM_REQFIELDS (sizeof(sv_reqfields) / sizeof(fields_t))

static fields_t sv_reqfields[] = 
{
	{141,	6,	"th_stand"},
	{142,	6,	"th_walk"},
	{143,	6,	"th_run"},
	{144,	6,	"th_pain"},
	{145,	6,	"th_die"},
	{146,	6,	"th_missile"},
	{147,	6,	"th_melee"},
	{148,	1,	"wad"},
	{149,	1,	"map"},
	{150,	1,	"landmark"},
	{151,	2,	"worldtype"},
	{152,	2,	"delay"},
	{153,	2,	"wait"},
	{154,	2,	"lip"},
	{155,	2,	"light_lev"},
	{156,	2,	"style"},
	{157,	2,	"skill"},
	{158,	1,	"killtarget"},
	{159,	3,	"pos1"},
	{162,	3,	"pos2"},
	{165,	3,	"mangle"},
	{168,	2,	"pain_finished"},
	{169,	2,	"air_finished"},
	{170,	3,	"camview"},
	{173,	2,	"aflag"},
	{174,	4,	"trigger_field"},
	{175,	2,	"anim_time"},
	{176,	2,	"anim_end"},
	{177,	2,	"anim_priority"},
	{178,	2,	"anim_run"},
	{179,	2,	"showhelp"},
	{180,	2,	"touched"},
	{181,	1,	"name"},
	{182,	4,	"triggerer"},
	{183,	1,	"targ"},
	{184,	1,	"targ[1]"},
	{185,	1,	"targ[2]"},
	{186,	1,	"targ[3]"},
	{187,	1,	"targ[4]"},
	{188,	1,	"targ[5]"},
	{189,	1,	"targ[6]"},
	{190,	1,	"targ[7]"},
	{191,	1,	"oldtarg"},
	{192,	1,	"oldtarg[1]"},
	{193,	1,	"oldtarg[2]"},
	{194,	1,	"oldtarg[3]"},
	{195,	1,	"oldtarg[4]"},
	{196,	1,	"oldtarg[5]"},
	{197,	1,	"oldtarg[6]"},
	{198,	1,	"oldtarg[7]"},
	{199,	2,	"twait"},
	{200,	2,	"twait[1]"},
	{201,	2,	"twait[2]"},
	{202,	2,	"twait[3]"},
	{203,	2,	"twait[4]"},
	{204,	2,	"twait[5]"},
	{205,	2,	"twait[6]"},
	{206,	2,	"twait[7]"},
	{207,	2,	"olddelay"},
	{208,	1,	"message1"},
	{209,	1,	"message2"},
	{210,	2,	"count"},
	{211,	2,	"state"},
	{212,	2,	"used"},
	{213,	3,	"dest"},
	{216,	1,	"target_dest"},
	{217,	1,	"oldmodel"}
};

#define PROG_CRC_SERVER	406

#endif//SV_EDICT_H