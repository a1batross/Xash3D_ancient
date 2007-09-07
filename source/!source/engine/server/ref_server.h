
// game.h -- game dll information visible to server
#include "savefile.h"

#define	GAME_API_VERSION	3

#define FL_TRACKTRAIN			0x00008000

// edict->svflags

#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER			0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER			0x00000004	// treat as CONTENTS_MONSTER for collision

#define AI_NOSTEP				0x00000400
#define AI_DUCKED				0x00000800


#define AI_ACTOR                		0x00040000

// edict->solid values
typedef enum
{
SOLID_NOT,		// no interaction with other objects
SOLID_TRIGGER,		// only touch when inside, after moving
SOLID_BBOX,		// touch on edge
SOLID_BSP			// bsp clip, touch on edge
} solid_t;

typedef enum {
	F_INT, 
	F_FLOAT,
	F_LSTRING,			// string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,			// string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
	F_EDICT,			// index on disk, pointer in memory
	F_ITEM,				// index on disk, pointer in memory
	F_CLIENT,			// index on disk, pointer in memory
	F_FUNCTION,
	F_MMOVE,
	F_IGNORE
} fieldtype_t;

typedef struct
{
	char		*name;
	int		ofs;
	fieldtype_t	type;
	int		flags;
} field_t;

typedef struct
{
	char		*name;
	void		(*spawn)(edict_t *ent);
} spawn_t;

//===============================================================

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

#define	MAX_ENT_CLUSTERS	16

struct gclient_s
{
	player_state_t		ps;		// communicated by server to clients
	int			ping;

	pmove_state_t		old_pmove;	// for detecting out-of-pmove changes
	vec3_t			v_angle;		// aiming direction

	vec3_t			oldviewangles;
	vec3_t			oldvelocity;

	float			bobtime;		// so off-ground doesn't change it

	// the game dll can add anything it wants after
	// this point in the structure
};

typedef struct monsterinfo_s
{
	int		aiflags;

	float		jumpup;
	float		jumpdn;
		
	void		(*jump)(edict_t *self);

} monsterinfo_t;

struct edict_s
{
	entity_state_t	s;
	struct gclient_s	*client;
	bool	inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf
	
	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================

	int		svflags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	solid_t		solid;
	int		clipmask;
	edict_t		*owner;

	// added for engine
	char		*classname;
	int		spawnflags;

	int		movetype;
	int		flags;

	char		*model;
	float		freetime;			// sv.time when the object was freed

	vec3_t		velocity;
	vec3_t		avelocity;
	vec3_t		origin_offset;

	int		health;

	float		nextthink;
	void		(*prethink) (edict_t *ent);
	void		(*think)(edict_t *self);
	void		(*blocked)(edict_t *self, edict_t *other);	//move to moveinfo?
	void		(*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
	void		(*use)(edict_t *self, edict_t *other, edict_t *activator);

	edict_t		*enemy;
	edict_t		*goalentity;
	edict_t		*movetarget;

	float		teleport_time;

	int		watertype;
	int		waterlevel;
	int		viewheight;

	monsterinfo_t	monsterinfo;

	vec3_t		movedir;
	edict_t		*groundentity;
	int		groundentity_linkcount;
	int		mass;

	float		gravity_debounce_time;
	float		gravity;

	int		takedamage;
	int		dmg;

	vec3_t		move_origin;
	vec3_t		move_angles;

	vec3_t		oldvelocity;

	float		speed;
	float		density;
	float		volume;		// precalculated size scale
	float		bob;		// bobbing in water amplitude
	float		duration;
	int		bobframe;
	int		bouncetype;
};

typedef struct game_export_s
{
	edict_t			*edicts;
	int			edict_size;
	int			client_size;
	int			num_edicts;		// current number, <= max_edicts
	int			max_edicts;

} game_export_t;

//dll handle
