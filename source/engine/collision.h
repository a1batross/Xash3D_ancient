//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       collision.h - base collision detect
//=======================================================================
#ifndef COLLISION_H
#define COLLISION_H

// content masks
#define MASK_ALL		(-1)
#define MASK_SOLID		(CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID	(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER		(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE		(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT		(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT	(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)
#define AREA_SOLID		1
#define AREA_TRIGGERS	2

// pmove->pm_flags
#define PMF_DUCKED		1
#define PMF_JUMP_HELD	2
#define PMF_ON_GROUND	4
#define PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define PMF_TIME_LAND	16	// pm_time is time before rejump
#define PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/
// button bits
#define	BUTTON_ATTACK		1
#define	BUTTON_USE		2
#define	BUTTON_ATTACK2		4
#define	BUTTONS_ATTACK		(BUTTON_ATTACK | BUTTON_ATTACK2)
#define	BUTTON_ANY		128 // any key whatsoever

extern float pm_airaccelerate;
void Pmove(pmove_t *pmove);

#endif//COLLISION_H