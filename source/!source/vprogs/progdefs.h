
/* File generated by FTEQCC, relevent for engine modding only, the generated crc must be the same as your engine expects. */

typedef struct globalvars_s
{	int pad;
	int ofs_return[3];
	int ofs_parm0[3];
	int ofs_parm1[3];
	int ofs_parm2[3];
	int ofs_parm3[3];
	int ofs_parm4[3];
	int ofs_parm5[3];
	int ofs_parm6[3];
	int ofs_parm7[3];
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
	func_t	SetNewParms;
	func_t	SetChangeParms;
} globalvars_t;

typedef struct entvars_s
{
	string_t	classname;
	string_t	globalname;
	float	modelindex;
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	velocity;
	vec3_t	avelocity;
	vec3_t	post_origin;
	vec3_t	post_angles;
	vec3_t	post_velocity;
	vec3_t	post_avelocity;
	vec3_t	origin_offset;
	vec3_t	angles_offset;
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
	float	colormap;
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
} entvars_t;

//with this the crc isn't needed for fields.
struct fieldvars_s
{
	int ofs;
	int type;
	char *name;
} fieldvars[] = {
	{0,	1,	"classname"},
	{1,	1,	"globalname"},
	{2,	2,	"modelindex"},
	{3,	3,	"origin"},
	{3,	2,	"origin_x"},
	{4,	2,	"origin_y"},
	{5,	2,	"origin_z"},
	{6,	3,	"angles"},
	{6,	2,	"angles_x"},
	{7,	2,	"angles_y"},
	{8,	2,	"angles_z"},
	{9,	3,	"velocity"},
	{9,	2,	"velocity_x"},
	{10,	2,	"velocity_y"},
	{11,	2,	"velocity_z"},
	{12,	3,	"avelocity"},
	{12,	2,	"avelocity_x"},
	{13,	2,	"avelocity_y"},
	{14,	2,	"avelocity_z"},
	{15,	3,	"post_origin"},
	{15,	2,	"post_origin_x"},
	{16,	2,	"post_origin_y"},
	{17,	2,	"post_origin_z"},
	{18,	3,	"post_angles"},
	{18,	2,	"post_angles_x"},
	{19,	2,	"post_angles_y"},
	{20,	2,	"post_angles_z"},
	{21,	3,	"post_velocity"},
	{21,	2,	"post_velocity_x"},
	{22,	2,	"post_velocity_y"},
	{23,	2,	"post_velocity_z"},
	{24,	3,	"post_avelocity"},
	{24,	2,	"post_avelocity_x"},
	{25,	2,	"post_avelocity_y"},
	{26,	2,	"post_avelocity_z"},
	{27,	3,	"origin_offset"},
	{27,	2,	"origin_offset_x"},
	{28,	2,	"origin_offset_y"},
	{29,	2,	"origin_offset_z"},
	{30,	3,	"angles_offset"},
	{30,	2,	"angles_offset_x"},
	{31,	2,	"angles_offset_y"},
	{32,	2,	"angles_offset_z"},
	{33,	2,	"ltime"},
	{34,	2,	"bouncetype"},
	{35,	2,	"movetype"},
	{36,	2,	"solid"},
	{37,	3,	"absmin"},
	{37,	2,	"absmin_x"},
	{38,	2,	"absmin_y"},
	{39,	2,	"absmin_z"},
	{40,	3,	"absmax"},
	{40,	2,	"absmax_x"},
	{41,	2,	"absmax_y"},
	{42,	2,	"absmax_z"},
	{43,	3,	"mins"},
	{43,	2,	"mins_x"},
	{44,	2,	"mins_y"},
	{45,	2,	"mins_z"},
	{46,	3,	"maxs"},
	{46,	2,	"maxs_x"},
	{47,	2,	"maxs_y"},
	{48,	2,	"maxs_z"},
	{49,	3,	"size"},
	{49,	2,	"size_x"},
	{50,	2,	"size_y"},
	{51,	2,	"size_z"},
	{52,	4,	"chain"},
	{53,	1,	"model"},
	{54,	2,	"frame"},
	{55,	2,	"sequence"},
	{56,	2,	"renderfx"},
	{57,	2,	"effects"},
	{58,	2,	"skin"},
	{59,	2,	"body"},
	{60,	1,	"weaponmodel"},
	{61,	2,	"weaponframe"},
	{62,	6,	"use"},
	{63,	6,	"touch"},
	{64,	6,	"think"},
	{65,	6,	"blocked"},
	{66,	6,	"activate"},
	{67,	6,	"walk"},
	{68,	6,	"jump"},
	{69,	6,	"duck"},
	{70,	2,	"flags"},
	{71,	2,	"aiflags"},
	{72,	2,	"spawnflags"},
	{73,	4,	"groundentity"},
	{74,	2,	"nextthink"},
	{75,	2,	"takedamage"},
	{76,	2,	"health"},
	{77,	2,	"frags"},
	{78,	2,	"weapon"},
	{79,	2,	"items"},
	{80,	1,	"target"},
	{81,	1,	"parent"},
	{82,	1,	"targetname"},
	{83,	4,	"aiment"},
	{84,	4,	"goalentity"},
	{85,	3,	"punchangle"},
	{85,	2,	"punchangle_x"},
	{86,	2,	"punchangle_y"},
	{87,	2,	"punchangle_z"},
	{88,	2,	"deadflag"},
	{89,	3,	"view_ofs"},
	{89,	2,	"view_ofs_x"},
	{90,	2,	"view_ofs_y"},
	{91,	2,	"view_ofs_z"},
	{92,	2,	"button0"},
	{93,	2,	"button1"},
	{94,	2,	"button2"},
	{95,	2,	"impulse"},
	{96,	2,	"fixangle"},
	{97,	3,	"v_angle"},
	{97,	2,	"v_angle_x"},
	{98,	2,	"v_angle_y"},
	{99,	2,	"v_angle_z"},
	{100,	2,	"idealpitch"},
	{101,	1,	"netname"},
	{102,	4,	"enemy"},
	{103,	2,	"colormap"},
	{104,	2,	"team"},
	{105,	2,	"max_health"},
	{106,	2,	"teleport_time"},
	{107,	2,	"armortype"},
	{108,	2,	"armorvalue"},
	{109,	2,	"waterlevel"},
	{110,	2,	"watertype"},
	{111,	2,	"ideal_yaw"},
	{112,	2,	"yaw_speed"},
	{113,	2,	"dmg_take"},
	{114,	2,	"dmg_save"},
	{115,	4,	"dmg_inflictor"},
	{116,	4,	"owner"},
	{117,	3,	"movedir"},
	{117,	2,	"movedir_x"},
	{118,	2,	"movedir_y"},
	{119,	2,	"movedir_z"},
	{120,	1,	"message"},
	{121,	2,	"sounds"},
	{122,	1,	"noise"},
	{123,	1,	"noise1"},
	{124,	1,	"noise2"},
	{125,	1,	"noise3"},
	{126,	2,	"jumpup"},
	{127,	2,	"jumpdn"},
	{128,	4,	"movetarget"},
	{129,	2,	"mass"},
	{130,	2,	"density"},
	{131,	2,	"gravity"},
	{132,	2,	"dmg"},
	{133,	2,	"dmgtime"},
	{134,	2,	"speed"}
};

#define PROGHEADER_CRC 42175
