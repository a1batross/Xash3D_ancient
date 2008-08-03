//=======================================================================
//			Copyright XashXT Group 2008 ©
//			sv_edict.h - server prvm edict
//=======================================================================
#ifndef SV_EDICT_H
#define SV_EDICT_H

struct sv_globalvars_s
{
	int	pad[34];
	int	pev;
	int	other;
	int	world;
	float	time;
	float	frametime;
	float	serverflags;
	string_t	mapname;
	string_t	startspot;
	vec3_t	spotoffset;
	float	deathmatch;
	float	teamplay;
	float	coop;
	float	total_secrets;
	float	found_secrets;
	float	total_monsters;
	float	killed_monsters;
	vec3_t	v_forward;
	vec3_t	v_right;
	vec3_t	v_up;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	float	trace_plane_dist;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_contents;
	float	trace_hitgroup;
	float	trace_flags;
	int	trace_ent;
	func_t	CreateAPI;
	func_t	StartFrame;
	func_t	EndFrame;
	func_t	PlayerPreThink;
	func_t	PlayerPostThink;
	func_t	ClientConnect;
	func_t	ClientDisconnect;
	func_t	PutClientInServer;
	func_t	ClientCommand;
	func_t	ClientUserInfoChanged;
};

struct sv_entvars_s
{
	string_t	classname;
	string_t	globalname;
	int	chain;
	func_t	precache;
	func_t	activate;
	func_t	blocked;
	func_t	touch;
	func_t	think;
	func_t	use;
	vec3_t	origin;
	vec3_t	angles;
	float	modelindex;
	vec3_t	old_origin;
	vec3_t	old_angles;
	vec3_t	velocity;
	vec3_t	avelocity;
	vec3_t	m_pcentre[3];
	vec3_t	m_pmatrix[4];
	vec3_t	movedir;
	vec3_t	force;
	vec3_t	torque;
	vec3_t	post_origin;
	vec3_t	post_angles;
	vec3_t	origin_offset;
	vec3_t	absmin;
	vec3_t	absmax;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;
	float	mass;
	float	solid;
	float	movetype;
	float	bouncetype;
	float	waterlevel;
	float	watertype;
	float	ltime;
	string_t	model;
	float	skin;
	float	body;
	float	alpha;
	float	frame;
	float	speed;
	float	animtime;
	float	sequence;
	float	effects;
	float	colormap;
	float	renderfx;
	float	flags;
	float	aiflags;
	float	spawnflags;
	vec3_t	view_ofs;
	vec3_t	v_angle;
	float	button0;
	float	button1;
	float	button2;
	float	impulse;
	vec3_t	punchangle;
	string_t	v_model;
	float	v_frame;
	float	v_body;
	float	v_skin;
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
	int	goalentity;
	float	fixangle;
	float	ideal_yaw;
	float	yaw_speed;
	float	teleport_time;
	int	groundentity;
	float	takedamage;
	float	nextthink;
	float	health;
	float	gravity;
	float	frags;
	float	team;
};


#define SV_NUM_REQFIELDS (sizeof(sv_reqfields) / sizeof(fields_t))

static fields_t sv_reqfields[] = 
{
	{144,	6,	"walk"},
	{145,	6,	"jump"},
	{146,	6,	"duck"},
	{147,	2,	"jump_flag"},
	{148,	2,	"swim_flag"},
	{149,	2,	"v_animtime"},
	{150,	2,	"weapon"},
	{151,	2,	"items"},
	{152,	1,	"target"},
	{153,	1,	"parent"},
	{154,	1,	"targetname"},
	{155,	2,	"deadflag"},
	{156,	2,	"idealpitch"},
	{157,	1,	"netname"},
	{158,	2,	"max_health"},
	{159,	2,	"armortype"},
	{160,	2,	"armorvalue"},
	{161,	2,	"dmg_take"},
	{162,	2,	"dmg_save"},
	{163,	4,	"dmg_inflictor"},
	{164,	1,	"message"},
	{165,	2,	"sounds"},
	{166,	1,	"noise"},
	{167,	1,	"noise1"},
	{168,	1,	"noise2"},
	{169,	1,	"noise3"},
	{170,	2,	"jumpup"},
	{171,	2,	"jumpdn"},
	{172,	4,	"movetarget"},
	{173,	2,	"density"},
	{174,	2,	"dmg"},
	{175,	2,	"dmgtime"},
	{176,	6,	"th_stand"},
	{177,	6,	"th_walk"},
	{178,	6,	"th_run"},
	{179,	6,	"th_pain"},
	{180,	6,	"th_die"},
	{181,	6,	"th_missile"},
	{182,	6,	"th_melee"},
	{183,	2,	"walkframe"},
	{184,	2,	"attack_finished"},
	{185,	2,	"pain_finished"},
	{186,	2,	"invincible_finished"},
	{187,	2,	"invisible_finished"},
	{188,	2,	"super_damage_finished"},
	{189,	2,	"radsuit_finished"},
	{190,	2,	"invincible_time"},
	{191,	2,	"invincible_sound"},
	{192,	2,	"invisible_time"},
	{193,	2,	"invisible_sound"},
	{194,	2,	"super_time"},
	{195,	2,	"super_sound"},
	{196,	2,	"rad_time"},
	{197,	2,	"fly_sound"},
	{198,	1,	"wad"},
	{199,	1,	"map"},
	{200,	1,	"landmark"},
	{201,	2,	"worldtype"},
	{202,	2,	"delay"},
	{203,	2,	"wait"},
	{204,	2,	"lip"},
	{205,	2,	"light_lev"},
	{206,	2,	"style"},
	{207,	2,	"skill"},
	{208,	1,	"killtarget"},
	{209,	1,	"noise4"},
	{210,	3,	"pos1"},
	{213,	3,	"pos2"},
	{216,	3,	"mangle"},
	{219,	2,	"count"},
	{220,	6,	"movedone"},
	{221,	3,	"finaldest"},
	{224,	3,	"finalangle"},
	{227,	2,	"t_length"},
	{228,	2,	"t_width"},
	{229,	3,	"dest"},
	{232,	3,	"dest1"},
	{235,	3,	"dest2"},
	{238,	2,	"state"},
	{239,	2,	"height"},
	{240,	2,	"cnt"},
	{241,	2,	"air_finished"},
	{242,	3,	"camview"},
	{245,	2,	"aflag"},
	{246,	4,	"trigger_field"},
	{247,	2,	"m_fSequenceLoops"},
	{248,	2,	"m_fSequenceFinished"},
	{249,	2,	"framerate"},
	{250,	2,	"anim_time"},
	{251,	2,	"anim_end"},
	{252,	2,	"anim_priority"},
	{253,	2,	"anim_run"},
	{254,	2,	"showhelp"},
	{255,	2,	"showinventory"},
	{256,	2,	"touched"},
	{257,	1,	"name"},
	{258,	4,	"triggerer"},
	{259,	2,	"used"},
	{260,	1,	"target_dest"},
	{261,	1,	"oldmodel"}
};

#define PROG_CRC_SERVER		1476

#endif//SV_EDICT_H