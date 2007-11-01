/*
** Copyright (C) 2003 Eric Lasota/Orbiter Productions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __ROQENC_LOCAL_H__
#define __ROQENC_LOCAL_H__

#include <stdlib.h>
#include <string.h>

#include "roqe.h"

// Force 32-bit mode
#define int long

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef double ROQDECIMAL;

typedef struct
{
	unsigned char yuv[6];
} yuvBlock2_t;

typedef struct
{
	yuvBlock2_t block[4];
} yuvBlock4_t;

typedef struct
{
	uchar indexes[4];
} yuvDiskBlock4_t;

typedef struct { unsigned char rgb[8*8*3]; } rgbBlock8_t;
typedef struct { unsigned char rgb[4*4*3]; } rgbBlock4_t;
typedef struct { unsigned char rgb[2*2*3]; } rgbBlock2_t;

typedef struct
{
	unsigned char *rgb;
} videoFrame_t;

typedef struct { int x,y; } motionVector_t;

typedef struct
{
	double bestDownscaleRate;
	int mse[259];
	int currentMSE;

	ulong currentSize;

	ushort currentChoice;
	ushort bestDownscaleChoice;
	uchar downscalePossible;

} possibilityList_t;


typedef struct roq_compressor_s
{
	int initialized;
	int wroteInfo;
	int frameNum;	// Hack to fix RoQ bug

	unsigned long preferences[ROQENC_PREF_COUNT];

	videoFrame_t frames[3];
	videoFrame_t *frameCycle[3];

	// Local data
	uint *devLUT8;
	uint *devLUT4;
	uint *devLUT2;
	uint *devMotion8;
	uint *devMotion4;
	uint *devSkip4;		// Skip8 can be calculated from this

	motionVector_t *motionVectors8;
	motionVector_t *motionVectors4;
	uchar *lut8;
	uchar *lut4;
	uchar *lut2;

	uchar *recon;		// Frame reconstruction after compression
	uchar *preWrite;

	possibilityList_t *pList;
	uint *listSort;

	int *optimizedOut4;

	int framesSinceKeyframe;
} roq_compressor_t;

extern roqenc_imports_t roqi;
extern roq_compressor_t *comp;
extern ulong vidWidth,vidHeight,vidScanWidth;
extern uchar *rgbFrame0;
extern uchar *rgbFrame1;
extern uchar *rgbFrame2;
extern uint refinementPasses;

extern rgbBlock8_t codebook8[256];
extern rgbBlock4_t codebook4[256];
extern rgbBlock2_t codebook2[256];
extern uint quickDiff[256*256];

extern yuvBlock2_t yuvCodebook2[256];
extern yuvDiskBlock4_t yuvDisk4[256];

extern int *optimizedOut4;

extern roq_stats_t stats;


#define QUICKDIFF(a,b) quickDiff[((a)<<8)+(b)]

#define PERTURBATION_BASE_POWER      6

void RoQEnc_InitQuickdiff(void);
int RoQEnc_GetCodebooks(void);
void RoQEnc_GenerateBlocks(void);
void RoQEnc_InitYUV(void);
ulong RoQEnc_CompressRGB(roq_compressor_t *compressor, void *file, unsigned char *rgb);


void YUV2toRGB2(uchar *yuv, uchar *rgb);
void RGB2toYUV2(uchar *rgb, uchar *yuv);

void YUVPerturb2(yuvBlock2_t *a, yuvBlock2_t *b, uint step);
uint YUVDifference2(yuvBlock2_t *a, yuvBlock2_t *b);
int YUVGenerateCodebooks4(yuvBlock4_t *input, uint inputCount, uint goalCells, uint *resultCount, yuvBlock4_t **resultElements);
int YUVGenerateCodebooks2(yuvBlock2_t *input, uint inputCount, uint goalCells, uint *resultCount, yuvBlock2_t **resultElements);
void Blit(uchar *source, uchar *dest, ulong scanWidth, ulong sourceSkip, ulong destSkip, ulong rows);
void DoubleSize(unsigned char *source, unsigned char *dest, unsigned long dim);

void Enc_RGBtoYUV(unsigned char r, unsigned char g, unsigned char b,
			  unsigned char *yp, unsigned char *up, unsigned char *vp);
void Enc_YUVtoRGB(unsigned char y, unsigned char u, unsigned char v,
			  unsigned char *rp, unsigned char *gp, unsigned char *bp);


#define CHUNKMARK_INFO          0x1001
#define CHUNKMARK_CODEBOOK      0x1002
#define CHUNKMARK_VIDEO         0x1011
#define CHUNKMARK_AUDIO_MONO    0x1020
#define CHUNKMARK_AUDIO_STEREO  0x1021

#endif
