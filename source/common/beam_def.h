//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        beam_def.h - view beam rendering
//=======================================================================
#ifndef BEAM_DEF_H
#define BEAM_DEF_H

// beam flags
#define FBEAM_STARTENTITY		0x00000001
#define FBEAM_ENDENTITY		0x00000002
#define FBEAM_FADEIN		0x00000004
#define FBEAM_FADEOUT		0x00000008
#define FBEAM_SINENOISE		0x00000010
#define FBEAM_SOLID			0x00000020
#define FBEAM_SHADEIN		0x00000040
#define FBEAM_SHADEOUT		0x00000080
#define FBEAM_ONLYNOISEONCE		0x00000100	// Only calculate our noise once
#define FBEAM_STARTVISIBLE		0x10000000	// Has this client actually seen this beam's start entity yet?
#define FBEAM_ENDVISIBLE		0x20000000	// Has this client actually seen this beam's end entity yet?
#define FBEAM_ISACTIVE		0x40000000
#define FBEAM_FOREVER		0x80000000

#define MAX_BEAM_ENTS		10
#define NOISE_DIVISIONS		128

struct beam_s
{
	BEAM		*next;
	HSPRITE		m_hSpriteTexture;		// actual sprite texture

	// bounding box for right culling on render-side
	vec3_t		mins;
	vec3_t		maxs;

	int		type;
	int		flags;

	// old-style source-target
	vec3_t		source;
	vec3_t		target;

	// control points for the beam
	int		numAttachments;
	vec3_t		attachment[MAX_BEAM_ENTS];
	vec3_t		delta;

	float		t;		// 0 .. 1 over lifetime of beam
	float		freq;
	float		die;
	float		width;
	float		endWidth;
	float		fadeLength;
	float		amplitude;
	float		life;
	float		r, g, b;
	float		brightness;
	float		speed;
	float		frameRate;
	float		frame;
	int		segments;

	// old-style source-target
	int		startEntity;
	int		endEntity;

	// attachment entities for the beam
	edict_t		*entity[MAX_BEAM_ENTS];
	int		attachmentIndex[MAX_BEAM_ENTS];

	int		modelIndex;
	int		frameCount;

	float		rgNoise[NOISE_DIVISIONS+1];
	BEAMTRAIL		*trail;

	// for TE_BEAMRINGPOINT
	float		start_radius;
	float		end_radius;

	// for FBEAM_ONLYNOISEONCE
	bool		m_bCalculatedNoise;
};

#endif//BEAM_DEF_H