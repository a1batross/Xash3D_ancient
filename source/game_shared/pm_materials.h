//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     pm_materials.h - defined texture types
//=======================================================================
#ifndef PM_MATERIALS_H
#define PM_MATERIALS_H

#define CBTEXTURENAMEMAX		13	// only load first n chars of name

#define CHAR_TEX_CONCRETE		'C'	// texture types
#define CHAR_TEX_METAL		'M'
#define CHAR_TEX_DIRT		'D'
#define CHAR_TEX_VENT		'V'
#define CHAR_TEX_GRATE		'G'
#define CHAR_TEX_TILE		'T'
#define CHAR_TEX_SLOSH		'S'
#define CHAR_TEX_WOOD		'W'
#define CHAR_TEX_COMPUTER		'P'
#define CHAR_TEX_GLASS		'Y'
#define CHAR_TEX_FLESH		'F'

#define STEP_CONCRETE		0	// default step sound
#define STEP_METAL			1	// metal floor
#define STEP_DIRT			2	// dirt, sand, rock
#define STEP_VENT			3	// ventillation duct
#define STEP_GRATE			4	// metal grating
#define STEP_TILE			5	// floor tiles
#define STEP_SLOSH			6	// shallow liquid puddle
#define STEP_WADE			7	// wading in liquid
#define STEP_LADDER			8	// climbing ladder

#endif//PM_MATERIALS_H