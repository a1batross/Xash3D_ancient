//=======================================================================
//			Copyright XashXT Group 2007 ©
//			studio.h - sprite model header
//=======================================================================
#ifndef SPRITE_H
#define SPRITE_H

/*
==============================================================================

SPRITE MODELS
==============================================================================
*/

//header
#define SPRITE_VERSION	2
#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSP"

//render format
#define SPR_VP_PARALLEL_UPRIGHT	0
#define SPR_FACING_UPRIGHT		1
#define SPR_VP_PARALLEL		2
#define SPR_ORIENTED		3
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL			0
#define SPR_ADDITIVE		1
#define SPR_INDEXALPHA		2
#define SPR_ALPHTEST		3

typedef enum { SPR_SINGLE = 0, SPR_GROUP } frametype_t;
typedef enum { ST_SYNC = 0, ST_RAND } synctype_t;

typedef struct
{
	int		ident;
	int		version;
	int		type;
	int		texFormat;
	float		boundingradius;
	int		width;
	int		height;
	int		numframes;
	float		beamlength;
	synctype_t	synctype;
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

typedef struct
{
	int		numframes;
} dspritegroup_t;

typedef struct
{
	float		interval;
} dspriteinterval_t;

typedef struct
{
	frametype_t	type;
} dspriteframetype_t;

#endif//SPRITE_H