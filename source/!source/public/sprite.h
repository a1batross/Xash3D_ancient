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
#define SPRITE_VERSION_HALF	2
#define SPRITE_VERSION_XASH	3
#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSP"

//render format
#define SPR_VP_PARALLEL_UPRIGHT	0
#define SPR_FACING_UPRIGHT		1
#define SPR_VP_PARALLEL		2
#define SPR_ORIENTED		3
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL			0 //solid sprite
#define SPR_ADDITIVE		1
#define SPR_INDEXALPHA		2
#define SPR_ALPHTEST		3
#define SPR_ADDGLOW			4 //same as additive, but without depthtest

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
	float		framerate; //xash auto-animate
	uint		rgbacolor; //packed rgba color
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

typedef struct
{
	int		type;
} frametype_t;

#endif//SPRITE_H