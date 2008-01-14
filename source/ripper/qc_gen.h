//=======================================================================
//			Copyright XashXT Group 2007 �
//		      qc_gen.h - sprite\model qc generator
//=======================================================================
#ifndef QC_GEN_H
#define QC_GEN_H

#include "basefiles.h"

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
	case SPR_NORMAL: return "normal";
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

#endif//QC_GEN_H