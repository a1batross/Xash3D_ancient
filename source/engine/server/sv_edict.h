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

struct sv_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool			free;
	float			freetime;	 	// sv.time when the object was freed

	// sv_private_edict_t
	worldsector_t		*worldsector;	// member of current wolrdsector
	struct sv_edict_s 		*nextedict;	// next edict in world sector
	struct sv_client_s		*client;		// filled for player ents
	int			clipmask;		// trace info
	int			lastcluster;	// unused if num_clusters != -1
	int			linkcount;
	int			num_clusters;	// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			areanum, areanum2;

	int			serialnumber;	// unical entity #id
	int			solid;		// see entity_state_t for details
	physbody_t		*physbody;	// ptr to phys body

	// baselines
	entity_state_t		s;
};

struct sv_globalvars_s
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
	func_t	ClientUserInfoChanged;
};

struct sv_entvars_s
{
	string_t	classname;
	string_t	globalname;
	float	modelindex;
	func_t	precache;
	func_t	activate;
	func_t	use;
	func_t	touch;
	func_t	think;
	func_t	blocked;
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	old_origin;
	vec3_t	old_angles;
	vec3_t	velocity;
	vec3_t	avelocity;
	vec3_t	m_pmatrix[4];
	vec3_t	m_pcentre[3];
	vec3_t	torque;
	vec3_t	movedir;
	vec3_t	force;
	vec3_t	post_origin;
	vec3_t	post_angles;
	vec3_t	origin_offset;
	vec3_t	absmin;
	vec3_t	absmax;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	float	solid;
	float	movetype;
	float	bouncetype;
	float	waterlevel;
	float	watertype;
	float	ltime;
	float	mass;
	int	chain;
	string_t	model;
	float	frame;
	float	body;
	float	skin;
	float	alpha;
	float	speed;
	float	sequence;
	float	animtime;
	float	effects;
	float	renderfx;
	float	flags;
	float	aiflags;
	float	spawnflags;
	vec3_t	punchangle;
	vec3_t	view_ofs;
	vec3_t	v_angle;
	string_t	v_model;
	float	v_frame;
	float	v_body;
	float	v_skin;
	vec3_t	v_offset;
	vec3_t	v_angles;
	float	v_sequence;
	string_t	p_model;
	float	p_frame;
	float	p_body;
	float	p_skin;
	float	p_sequence;
	string_t	loopsound;
	float	loopsndvol;
	float	loopsndattn;
	int	owner;
	int	enemy;
	int	aiment;
	float	ideal_yaw;
	float	yaw_speed;
	float	teleport_time;
	int	groundentity;
	float	takedamage;
	float	nextthink;
	float	health;
	float	gravity;
	float	team;
};

#define SV_NUM_REQFIELDS (sizeof(sv_reqfields) / sizeof(fields_t))

static fields_t sv_reqfields[] = 
{
	{142,	6,	"walk"},
	{143,	6,	"jump"},
	{144,	6,	"duck"},
	{145,	2,	"v_animtime"},
	{146,	2,	"frags"},
	{147,	2,	"weapon"},
	{148,	2,	"items"},
	{149,	1,	"target"},
	{150,	1,	"parent"},
	{151,	1,	"targetname"},
	{152,	4,	"goalentity"},
	{153,	2,	"deadflag"},
	{154,	2,	"button0"},
	{155,	2,	"button1"},
	{156,	2,	"button2"},
	{157,	2,	"impulse"},
	{158,	2,	"fixangle"},
	{159,	2,	"idealpitch"},
	{160,	1,	"netname"},
	{161,	2,	"max_health"},
	{162,	2,	"armortype"},
	{163,	2,	"armorvalue"},
	{164,	2,	"dmg_take"},
	{165,	2,	"dmg_save"},
	{166,	4,	"dmg_inflictor"},
	{167,	1,	"message"},
	{168,	2,	"sounds"},
	{169,	1,	"noise"},
	{170,	1,	"noise1"},
	{171,	1,	"noise2"},
	{172,	1,	"noise3"},
	{173,	2,	"jumpup"},
	{174,	2,	"jumpdn"},
	{175,	4,	"movetarget"},
	{176,	2,	"density"},
	{177,	2,	"dmg"},
	{178,	2,	"dmgtime"},
	{179,	6,	"th_stand"},
	{180,	6,	"th_walk"},
	{181,	6,	"th_run"},
	{182,	6,	"th_pain"},
	{183,	6,	"th_die"},
	{184,	6,	"th_missile"},
	{185,	6,	"th_melee"},
	{186,	2,	"walkframe"},
	{187,	2,	"attack_finished"},
	{188,	2,	"pain_finished"},
	{189,	2,	"invincible_finished"},
	{190,	2,	"invisible_finished"},
	{191,	2,	"super_damage_finished"},
	{192,	2,	"radsuit_finished"},
	{193,	2,	"invincible_time"},
	{194,	2,	"invincible_sound"},
	{195,	2,	"invisible_time"},
	{196,	2,	"invisible_sound"},
	{197,	2,	"super_time"},
	{198,	2,	"super_sound"},
	{199,	2,	"rad_time"},
	{200,	2,	"fly_sound"},
	{201,	1,	"wad"},
	{202,	1,	"map"},
	{203,	1,	"landmark"},
	{204,	2,	"worldtype"},
	{205,	2,	"delay"},
	{206,	2,	"wait"},
	{207,	2,	"lip"},
	{208,	2,	"light_lev"},
	{209,	2,	"style"},
	{210,	2,	"skill"},
	{211,	1,	"killtarget"},
	{212,	1,	"noise4"},
	{213,	3,	"pos1"},
	{216,	3,	"pos2"},
	{219,	3,	"mangle"},
	{222,	2,	"count"},
	{223,	6,	"movedone"},
	{224,	3,	"finaldest"},
	{227,	3,	"finalangle"},
	{230,	2,	"t_length"},
	{231,	2,	"t_width"},
	{232,	3,	"dest"},
	{235,	3,	"dest1"},
	{238,	3,	"dest2"},
	{241,	2,	"state"},
	{242,	2,	"height"},
	{243,	2,	"cnt"},
	{244,	2,	"air_finished"},
	{245,	3,	"camview"},
	{248,	2,	"aflag"},
	{249,	4,	"trigger_field"},
	{250,	2,	"m_fSequenceLoops"},
	{251,	2,	"m_fSequenceFinished"},
	{252,	2,	"framerate"},
	{253,	2,	"anim_time"},
	{254,	2,	"anim_end"},
	{255,	2,	"anim_priority"},
	{256,	2,	"anim_run"},
	{257,	2,	"showhelp"},
	{258,	2,	"showinventory"},
	{259,	2,	"touched"},
	{260,	1,	"name"},
	{261,	4,	"triggerer"},
	{262,	2,	"used"},
	{263,	1,	"target_dest"},
	{264,	1,	"oldmodel"}
};

#endif//SV_EDICT_H