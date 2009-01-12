//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     trace_def.h - model event definition
//=======================================================================
#ifndef TRACE_DEF_H
#define TRACE_DEF_H

// monster's walkmove modes
enum
{
	WALKMOVE_NORMAL = 0,
	WALKMOVE_NOMONSTERS,
	WALKMOVE_MISSILE,
	WALKMOVE_WORLDONLY,
	WALKMOVE_HITMODEL,
	WALKMOVE_CHECKONLY,
};

// bsp contents
typedef enum
{
	CONTENTS_NONE		= 0, 	// just a mask for source tabulation
	CONTENTS_SOLID		= BIT(0),	// an eye is never valid in a solid
	CONTENTS_WINDOW		= BIT(1),	// translucent, but not watery
	CONTENTS_AUX		= BIT(2),
	CONTENTS_LAVA		= BIT(3),
	CONTENTS_SLIME		= BIT(4),
	CONTENTS_WATER		= BIT(5),
	CONTENTS_SKY		= BIT(6),
	
	// space for new user contents

	CONTENTS_MIST		= BIT(12),// g-cont. what difference between fog and mist ?
	LAST_VISIBLE_CONTENTS	= BIT(12),// mask (LAST_VISIBLE_CONTENTS-1)
	CONTENTS_FOG		= BIT(13),// future expansion
	CONTENTS_AREAPORTAL		= BIT(14),// func_areaportal volume
	CONTENTS_PLAYERCLIP		= BIT(15),// clip affect only by player or bot
	CONTENTS_MONSTERCLIP	= BIT(16),// clip affect only by monster or npc
	CONTENTS_CLIP		= (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP), // both type clip
	CONTENTS_ORIGIN		= BIT(17),// removed before bsping an entity
	CONTENTS_BODY		= BIT(18),// should never be on a brush, only in game
	CONTENTS_CORPSE		= BIT(19),// deadbody
	CONTENTS_DETAIL		= BIT(20),// brushes to be added after vis leafs
	CONTENTS_TRANSLUCENT	= BIT(21),// auto set if any surface has trans
	CONTENTS_LADDER		= BIT(22),// like water but ladder : )
	CONTENTS_TRIGGER		= BIT(23),// trigger volume

	// content masks
	MASK_SOLID		= (CONTENTS_SOLID|CONTENTS_WINDOW),
	MASK_PLAYERSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_MONSTERSOLID		= (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_DEADSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_WINDOW),
	MASK_WATER		= (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME),
	MASK_OPAQUE		= (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA),
	MASK_SHOT			= (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_WINDOW|CONTENTS_CORPSE)
} contentType_t;

// NOTE: engine trace struct not matched with game trace
typedef struct
{
	BOOL		fAllSolid;	// if true, plane is not valid
	BOOL		fStartSolid;	// if true, the initial point was in a solid area
	BOOL		fStartStuck;	// if true, trace started from solid entity
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	int		iStartContents;	// start pos conetnts
	int		iContents;	// final pos contents
	int		iHitgroup;	// 0 == generic, non zero is specific body part
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	const char	*pTexName;	// texture name that we hitting (brushes and studiomodels)
	edict_t		*pHit;		// entity the surface is on
} TraceResult;

#endif//TRACE_DEF_H