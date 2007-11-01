//=======================================================================
//			Copyright XashXT Group 2007 ©
//			  roqlib.h - roq video maker
//=======================================================================
#ifndef ROQLIB_H
#define ROQLIB_H

#include "platform.h"
#include "utils.h"

#define ROQ_AUDIO_INITIALIZED		1
#define ROQ_VIDEO_INITIALIZED		2
#define AUDIO_SAMPLE_MAX		22050	// Audio chunks can contain up to 1 second of stereo audio

typedef byte block8[8*8*3];
typedef byte block4[4*4*3];
typedef byte block2[2*2*3];

typedef struct roq_state_s
{
	block8	cb4large[256];

	block4	cb4[256];
	block2	cb2[256];

	dword	maxVideo;

	file_t	*cinematic;

	byte	*videoMemory;		// The only actual allocated buffer
	byte	*videoChunk;
	byte	*videoBuffers[2];		// front and back buffer
	int	currentBuffer;
	byte	audioChunk[AUDIO_SAMPLE_MAX*2];
	short	audioSamples[AUDIO_SAMPLE_MAX*2];

} roq_state_t;

extern byte *roqpool;
#define RQalloc( size )	Mem_Alloc(roqpool, size )

// utils
void ROQ_Blit(byte *source, byte *dest, dword scanWidth, dword sourceSkip, dword destSkip, dword rows);
void ROQ_DoubleSize(byte *source, byte *dest, dword dim);

#endif//ROQLIB_H