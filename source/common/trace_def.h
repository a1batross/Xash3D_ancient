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

	CONTENTS_SKY		= BIT(2),
	CONTENTS_LAVA		= BIT(3),
	CONTENTS_SLIME		= BIT(4),
	CONTENTS_WATER		= BIT(5),
	CONTENTS_FOG		= BIT(6),
	
	// space for new user contents
	CONTENTS_AREAPORTAL		= BIT(15),
	CONTENTS_PLAYERCLIP		= BIT(16),
	CONTENTS_MONSTERCLIP	= BIT(17),
	CONTENTS_CLIP		= (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP), // both type clip
	CONTENTS_TELEPORTER		= BIT(18),
	CONTENTS_JUMPPAD		= BIT(19),
	CONTENTS_CLUSTERPORTAL	= BIT(20),
	CONTENTS_DONOTENTER		= BIT(21),
	CONTENTS_ORIGIN		= BIT(22),// removed before bsping an entity

	CONTENTS_BODY		= BIT(23),// should never be on a brush, only in game
	CONTENTS_CORPSE		= BIT(24),
	CONTENTS_DETAIL		= BIT(25),// brushes not used for the bsp
	CONTENTS_STRUCTURAL		= BIT(26),// brushes used for the bsp
	CONTENTS_TRANSLUCENT	= BIT(27),// don't consume surface fragments inside
	CONTENTS_TRIGGER		= BIT(28),// trigger volume
	CONTENTS_NODROP		= BIT(29),// don't leave bodies or items (death fog, lava)

	// content masks
	MASK_SOLID		= (CONTENTS_SOLID),
	MASK_PLAYERSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY),
	MASK_MONSTERSOLID		= (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BODY),
	MASK_DEADSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP),
	MASK_WATER		= (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME),
	MASK_OPAQUE		= (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA),
	MASK_SHOT			= (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE)
} contentType_t;

// NOTE: engine trace struct not matched with game trace
typedef struct
{
	BOOL		fAllSolid;	// if true, plane is not valid
	BOOL		fStartSolid;	// if true, the initial point was in a solid area
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	int		iContents;	// final pos contents
	int		iHitgroup;	// 0 == generic, non zero is specific body part
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	const char	*pTexName;	// texture name that we hitting (brushes and studiomodels)
	edict_t		*pHit;		// entity the surface is on
} TraceResult;

#endif//TRACE_DEF_H