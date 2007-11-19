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

// edict->solid values
typedef enum
{
	SOLID_NOT,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH,	// custom convex hull
} solid_t;

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev;
	struct link_s	*next;
	int		entnum; // get edict by number
} link_t;

struct gclient_s
{
	player_state_t		ps;		// communicated by server to clients
	int			ping;

	pmove_state_t		old_pmove;	// for detecting out-of-pmove changes
	vec3_t			v_angle;		// aiming direction

	vec3_t			oldviewangles;
	vec3_t			oldvelocity;

	float			bobtime;		// so off-ground doesn't change it
};

struct sv_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool			free;
	float			freetime;	 	// sv.time when the object was freed

	// sv_private_edict_t
	link_t			area;		// linked to a division node or leaf
	int			clipmask;		// trace info
	int			headnode;		// unused if num_clusters != -1
	int			linkcount;
	int			num_clusters;	// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			areanum, areanum2;
	int			serialnumber;	// unical entity #id
	int			solid;		// see entity_state_t for details
	int			event;		// apply sv.events too
	NewtonBody		*physbody;	// ptr to phys body
	NewtonCollision		*collision;	// collision callback

	// baselines
	entity_state_t		s;
	struct gclient_s		*client;		//get rid of this
};
#endif//SV_EDICT_H