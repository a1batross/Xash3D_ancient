//=======================================================================
//			Copyright XashXT Group 2010 ©
//		        beam_def.h - view beam rendering
//=======================================================================
#ifndef BEAM_DEF_H
#define BEAM_DEF_H

// beam types
typedef enum 
{
	BEAM_POINTS = 0,
	BEAM_ENTPOINT,
	BEAM_ENTS,
	BEAM_HOSE,
	BEAM_LASER,
} kBeamType_t;

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

// Start/End Entity is encoded as 12 bits of entity index, and 4 bits of attachment (4:12)
#define BEAMENT_ENTITY( x )		((x) & 0xFFF)
#define BEAMENT_ATTACHMENT( x )	(((x)>>12) & 0xF)

#endif//BEAM_DEF_H