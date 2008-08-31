//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      qc_gen.h - sprite\model qc generator
//=======================================================================
#ifndef QC_GEN_H
#define QC_GEN_H

#include "byteorder.h"

// sprite types
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

// q2 wal contents
#define CONTENTS_SOLID	0x00000001	// an eye is never valid in a solid
#define CONTENTS_WINDOW	0x00000002	// translucent, but not watery
#define CONTENTS_AUX	0x00000004
#define CONTENTS_LAVA	0x00000008
#define CONTENTS_SLIME	0x00000010
#define CONTENTS_WATER	0x00000020
#define CONTENTS_MIST	0x00000040

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_AREAPORTAL	0x00008000
#define CONTENTS_PLAYERCLIP	0x00010000
#define CONTENTS_MONSTERCLIP	0x00020000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0	0x00040000
#define CONTENTS_CURRENT_90	0x00080000
#define CONTENTS_CURRENT_180	0x00100000
#define CONTENTS_CURRENT_270	0x00200000
#define CONTENTS_CURRENT_UP	0x00400000
#define CONTENTS_CURRENT_DOWN	0x00800000

#define CONTENTS_ORIGIN	0x01000000	// removed before BSP'ing an entity

#define CONTENTS_MONSTER	0x02000000	// should never be on a brush, only in game
#define CONTENTS_DEADMONSTER	0x04000000
#define CONTENTS_DETAIL	0x08000000	// brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define CONTENTS_LADDER	0x20000000

#define SURF_LIGHT		0x00000001	// value will hold the light strength
#define SURF_SLICK		0x00000002	// effects game physics
#define SURF_SKY		0x00000004	// don't draw, but add to skybox
#define SURF_WARP		0x00000008	// turbulent water warp
#define SURF_TRANS33	0x00000010	// 33% opacity
#define SURF_TRANS66	0x00000020	// 66% opacity
#define SURF_FLOWING	0x00000040	// scroll towards angle
#define SURF_NODRAW		0x00000080	// don't bother referencing the texture
#define SURF_HINT		0x00000100	// make a primary bsp splitter
#define SURF_SKIP		0x00000200	// completely ignore, allowing non-closed brushes

// xash 0.45 surfaces replacement table
#define SURF_MIRROR		0x00010000	// mirror surface
#define SURF_PORTAL		0x00020000	// portal surface

//
// sprite_qc
//
typedef struct frame_s
{
	char	name[64];		// framename

	int	width;		// lumpwidth
	int	height;		// lumpheight
	int	origin[2];	// monster origin
} frame_t;

typedef struct group_s
{
	frame_t	frame[64];	// max groupframes
	float	interval[64];	// frame intervals
	int	numframes;	// num group frames;
} group_t;

struct qcsprite_s
{
	group_t	group[8];		// 64 frames for each group
	frame_t	frame[512];	// or 512 ungroupped frames

	int	type;		// rendering type
	int	texFormat;	// half-life texture
	bool	truecolor;	// spr32
	byte	palette[256][4];	// shared palette

	int	numgroup;		// groups counter
	int	totalframes;	// including group frames
} spr;

//
// doom spritemodel_qc
//
typedef struct angled_s
{
	char	name[10];		// copy of skin name

	int	width;		// lumpwidth
	int	height;		// lumpheight
	int	origin[2];	// monster origin
	byte	xmirrored;	// swap left and right
} angled_t;

struct angledframe_s
{
	angled_t	frame[8];		// angled group or single frame
	int	bounds[2];	// group or frame maxsizes
	byte	angledframes;	// count of angled frames max == 8
	byte	normalframes;	// count of anim frames max == 1
	byte	mirrorframes;	// animation mirror stored

	char	membername[8];	// current model name, four characsters
	char	animation;	// current animation number
	bool	in_progress;	// current state
} flat;

_inline const char *SPR_RenderMode( void )
{
	switch( spr.texFormat )
	{
	case SPR_ADDGLOW: return "glow";
	case SPR_ALPHTEST: return "alphatest";
	case SPR_INDEXALPHA: return "indexalpha";
	case SPR_ADDITIVE: return "additive";
	case SPR_NORMAL: return "solid";
	default: return "normal";
	}
}

_inline const char *SPR_RenderType( void )
{
	switch( spr.type )
	{
	case SPR_ORIENTED: return "oriented";
	case SPR_VP_PARALLEL: return "vp_parallel";
	case SPR_FACING_UPRIGHT: return "facing_upright";
	case SPR_VP_PARALLEL_UPRIGHT: return "vp_parallel_upright";
	case SPR_VP_PARALLEL_ORIENTED: return "vp_parallel_oriented";
	default: return "oriented";
	}
}

void Skin_FinalizeScript( void );
void Skin_CreateScript( const char *wad, const char *name, rgbdata_t *pic );
bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *ext, const char *anim, int surf, int cnt );
void Conv_GetPaletteQ2( void );
void Conv_GetPaletteQ1( void );
bool Conv_CheckMap( const char *mapname );
bool Conv_CheckWad( const char *wadname );
extern uint *d_currentpal;

#endif//QC_GEN_H