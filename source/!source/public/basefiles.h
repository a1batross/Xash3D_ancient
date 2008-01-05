//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        basefiles.h - xash supported formats
//=======================================================================
#ifndef BASE_FILES_H
#define BASE_FILES_H

/*
==============================================================================

SPRITE MODELS

.spr extended version (non-paletted 32-bit sprites with zlib-compression for each frame)
==============================================================================
*/

#define IDSPRITEHEADER		(('R'<<24)+('P'<<16)+('S'<<8)+'I')	// little-endian "ISPR"
#define SPRITE_VERSION		3

#define SPR_VP_PARALLEL_UPRIGHT	0
#define SPR_FACING_UPRIGHT		1
#define SPR_VP_PARALLEL		2
#define SPR_ORIENTED		3	// all axis are valid
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL			0	// solid sprite
#define SPR_ADDITIVE		1
#define SPR_INDEXALPHA		2
#define SPR_ALPHTEST		3
#define SPR_ADDGLOW			4	// same as additive, but without depthtest

typedef enum { ST_SYNC = 0, ST_RAND } synctype_t;
typedef enum { SPR_SINGLE = 0, SPR_GROUP, SPR_ANGLED } frametype_t;

typedef struct
{
	int		ident;
	int		version;
	int		type;
	int		rendermode;	// was boundingradius
	int		width;
	int		height;
	int		numframes;
	float		beamlength;	// not used
	synctype_t	synctype;
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
	int		compsize;
} dframe_t;

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
} dframetype_t;

#endif//BASE_FILES_H