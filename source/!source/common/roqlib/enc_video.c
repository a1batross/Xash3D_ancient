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

/********************************************************************

  SwitchBlade2 VQ Video Encoder process:
  1 - Generate codebooks or load them from the host's cache
  2 - If not a keyframe, find the best motion vectors for all blocks
  3 - If 2 or more frames from a keyframe, check Skip possibilities
  4 - Generate a list of the MSE of all possible encodings of each
      8x8 block.
  5 - Reduce the list by taking the cell with the lowest MSE gain for
      bits saved until MSE/bits is a positive number, and the total
      size fits within the target rate.


********************************************************************/

#include "roqe_local.h"

#define BLOCKMARK_SKIP   0
#define BLOCKMARK_MOTION 1
#define BLOCKMARK_VECTOR 2
#define BLOCKMARK_QUAD   3

#define MAX_MSE          99999999

#define UNMASK(x,y) ((x>>y)&3)


static uint bitUsage[4] = {
	2,			// Skip has no parameters
	2+8,		// Motion has two 4-bit motion parameters
	2+8,		// Vector uses a single vector
	2+(8*4),	// Quads contain 4 codebook entries
};
#define SUBQUAD_BIT_USAGE   2	// 2 bits needed for a QUAD tag preceding 4 others
#define DEFAULT_PLIST_ENTRY 258	// QUAD (QUAD QUAD QUAD QUAD) = The largest one

// Bit consumption for non-codebook entries
#define CB4ENTRYSIZE     (4*8)
#define CB2ENTRYSIZE     (6*8)
#define CBSIZE           (CB4ENTRYSIZE*256 + CB2ENTRYSIZE*256)
#define CHUNKHEADERSIZE  (8*8)
#define INFOBLOCKSIZE    (8*8)

roq_compressor_t *comp;
ulong vidWidth,vidHeight,vidScanWidth;
ulong cullThreshold;

uchar *rgbFrame0;
uchar *rgbFrame1;
uchar *rgbFrame2;

int *optimizedOut4;

rgbBlock8_t codebook8[256];
rgbBlock4_t codebook4[256];
rgbBlock2_t codebook2[256];

// Encoding data

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

uchar *recon;	// Reconstruction

uchar *preWrite;

possibilityList_t *pList;

uint *listSort;		// To dodge memory allocation SNAFU
uint *chosenPossibilities;

// WARNING: listSort and chosenPossibilities SHARE THE SAME MEMORY, since neither will be used
// at the same time as the other and both contain the same type of data.  IF LISTSORT IS
// REMOVED, THEN MEMORY MUST BE ALLOCATED FOR CHOSENPOSSIBILITIES!!!

uint refinementPasses;

int bookentryUsed2[256];
int bookentryUsed4[256];
uchar bookentryRemap2[256];
uchar bookentryRemap4[256];

int entriesUsed2;
int entriesUsed4;

uint quickDiff[256*256];	// Quick square-difference table

roq_stats_t stats;

void RoQEnc_InitQuickdiff(void)
{
	uint i,j;

	for(i=0;i<256;i++) for(j=0;j<256;j++) quickDiff[(i<<8)+j] = (i-j)*(i-j);
}



#define INIT_ALLOC(x, y) (x) = malloc(y); if(!(x)) failed = 1

static int RoQEnc_InitializeCompressor(void)
{
	int i;
	int failed;
	uint numBlocks;
	uint prewriteSize;

	vidWidth = comp->preferences[ROQENC_PREF_WIDTH];
	vidHeight = comp->preferences[ROQENC_PREF_HEIGHT];
	vidScanWidth = vidWidth * 3;

	if(!(vidWidth && vidHeight))
		return 0;	// Bad dimensions
	if((vidWidth & 15) || (vidHeight & 15))
		return 0;	// Bad dimensions
	if(vidWidth > 32768 || vidHeight > 32768)
		return 0;	// Too big

	// Set up data banks
	failed = 0;
	for(i=0;i<3;i++)
	{
		INIT_ALLOC(comp->frames[i].rgb, vidWidth*vidHeight*3);
	}

	INIT_ALLOC(comp->devLUT8, vidWidth*vidHeight*sizeof(uint)/(8*8));
	INIT_ALLOC(comp->devLUT4, vidWidth*vidHeight*sizeof(uint)/(4*4));
	INIT_ALLOC(comp->devLUT2, vidWidth*vidHeight*sizeof(uint)/(2*2));
	INIT_ALLOC(comp->devMotion8, vidWidth*vidHeight*sizeof(uint)/(8*8));
	INIT_ALLOC(comp->devMotion4, vidWidth*vidHeight*sizeof(uint)/(4*4));
	INIT_ALLOC(comp->devSkip4, vidWidth*vidHeight*sizeof(uint)/(4*4));

	INIT_ALLOC(comp->optimizedOut4, vidWidth*vidHeight*sizeof(int)/(4*4));

	INIT_ALLOC(comp->motionVectors8, vidWidth*vidHeight*sizeof(motionVector_t)/(8*8));
	INIT_ALLOC(comp->motionVectors4, vidWidth*vidHeight*sizeof(motionVector_t)/(4*4));

	INIT_ALLOC(comp->lut8, vidWidth*vidHeight/(8*8));
	INIT_ALLOC(comp->lut4, vidWidth*vidHeight/(4*4));
	INIT_ALLOC(comp->lut2, vidWidth*vidHeight/(2*2));

	INIT_ALLOC(comp->pList, vidWidth*vidHeight*sizeof(possibilityList_t)/(8*8));
	INIT_ALLOC(comp->listSort, vidWidth*vidHeight*sizeof(uint)/(8*8));

	INIT_ALLOC(comp->recon, vidWidth*vidHeight*3);


	numBlocks = vidWidth*vidHeight/64;
	prewriteSize = 0;

	// Allocate prewrite for the info chunk, codebook chunk, and the largest possible video chunk
	prewriteSize += CHUNKHEADERSIZE + INFOBLOCKSIZE;
	prewriteSize += CHUNKHEADERSIZE + CBSIZE;
	prewriteSize += CHUNKHEADERSIZE + numBlocks*(SUBQUAD_BIT_USAGE+(bitUsage[BLOCKMARK_QUAD]*4));

	// Allocate enough space with 4 bytes of overflow padding
	INIT_ALLOC(comp->preWrite, prewriteSize/8 + 4);

	comp->frameCycle[0] = comp->frames;
	comp->frameCycle[1] = comp->frames+1;
	comp->frameCycle[2] = comp->frames+2;

	comp->wroteInfo = 0;

	comp->frameNum = 0;

	RoQEnc_InitQuickdiff();
	RoQEnc_InitYUV();

	comp->initialized = 1;

	return !failed;
}

// RGB2toYUV2 :: Converts a 2x2 RGB block into a 4:2:0 YUV block
void RGB2toYUV2(uchar *rgb, uchar *yuv)
{
	uchar y[4];
	uchar u[4];
	uchar v[4];

	ushort utotal;
	ushort vtotal;

	Enc_RGBtoYUV(rgb[0], rgb[1], rgb[2],   y+0, u+0, v+0);
	Enc_RGBtoYUV(rgb[3], rgb[4], rgb[5],   y+1, u+1, v+1);
	Enc_RGBtoYUV(rgb[6], rgb[7], rgb[8],   y+2, u+2, v+2);
	Enc_RGBtoYUV(rgb[9], rgb[10], rgb[11], y+3, u+3, v+3);

	utotal = (ushort)u[0] + (ushort)u[1] + (ushort)u[2] + (ushort)u[3];
	vtotal = (ushort)v[0] + (ushort)v[1] + (ushort)v[2] + (ushort)v[3];

	yuv[0] = y[0];
	yuv[1] = y[1];
	yuv[2] = y[2];
	yuv[3] = y[3];

	yuv[4] = (uchar)(utotal/4);
	yuv[5] = (uchar)(vtotal/4);
}


// RGB2toYUV2 :: Converts a YUV 4:2:0 block into a 2x2 RGB block
void YUV2toRGB2(uchar *yuv, uchar *rgb)
{
	Enc_YUVtoRGB(yuv[0], yuv[4], yuv[5], rgb+0, rgb+1, rgb+2);
	Enc_YUVtoRGB(yuv[1], yuv[4], yuv[5], rgb+3, rgb+4, rgb+5);
	Enc_YUVtoRGB(yuv[2], yuv[4], yuv[5], rgb+6, rgb+7, rgb+8);
	Enc_YUVtoRGB(yuv[3], yuv[4], yuv[5], rgb+9, rgb+10, rgb+11);
}


// Deviation table generation
static uint Diff(uchar *a, uchar *b, uint len)
{
	uint diff = 0;

	while(len)
	{
		diff += quickDiff[((*a++)<<8) + (*b++)];
		len--;
	}
	return diff;
}

static uint CappedDiff(uchar *a, uchar *b, uint len, uint max)
{
	uint diff = 0;

	while(len)
	{
		diff += quickDiff[((*a++)<<8) + (*b++)];
		if(diff > max)
			break;
		len--;
	}
	return diff;
}


static void MakeDevLut8(void)
{
	uint x,y,n;
	uint i;
	uint pick;
	uint lowDiff;
	uint diff;

	uchar comparison[8*8*3];

	n = 0;
	for(y=0;y<vidHeight;y+=8)
	{
		for(x=0;x<vidWidth;x+=8)
		{
			Blit(rgbFrame0 + (x + (y*vidWidth))*3, comparison, 8*3, vidScanWidth, 8*3, 8);

			// Compare to each entry
			pick=0;
			lowDiff=99999999;
			for(i=0;i<256;i++)
			{
				diff = CappedDiff(codebook8[i].rgb, comparison, 8*8*3, lowDiff);
				if(diff < lowDiff) { pick=i; lowDiff=diff; }
			}
			lut8[n]=(uchar)pick;  devLUT8[n++]=lowDiff;
		}
	}
}

static void MakeDevLut4(void)
{
	uint x,y,n;
	uchar comparison[4*4*3];
	uint i;
	uint pick;
	uint lowDiff;
	uint diff;

	n = 0;
	for(y=0;y<vidHeight;y+=4)
	{
		for(x=0;x<vidWidth;x+=4)
		{
			Blit(rgbFrame0 + (x + (y*vidWidth))*3, comparison, 4*3, vidScanWidth, 4*3, 4);

			// Compare to each entry
			pick=0;
			lowDiff=99999999;
			for(i=0;i<256;i++)
			{
				diff = CappedDiff(codebook4[i].rgb, comparison, 4*4*3, lowDiff);
				if(diff < lowDiff) { pick=i; lowDiff=diff; }
			}
			lut4[n]=(uchar)pick;  devLUT4[n++]=lowDiff;
		}
	}
}

static void MakeDevLut2(void)
{
	uint x,y,n;
	uchar comparison[2*2*3];
	uint i;
	uint pick;
	uint lowDiff;
	uint diff;

	n = 0;
	for(y=0;y<vidHeight;y+=2)
	{
		for(x=0;x<vidWidth;x+=2)
		{
			Blit(rgbFrame0 + (x + (y*vidWidth))*3, comparison, 2*3, vidScanWidth, 2*3, 2);

			// Compare to each entry
			pick=0;
			lowDiff=99999999;
			for(i=0;i<256;i++)
			{
				diff = CappedDiff(codebook2[i].rgb, comparison, 2*2*3, lowDiff);
				if(diff < lowDiff) { pick=i; lowDiff=diff; }
			}
			lut2[n]=(uchar)pick;  devLUT2[n++]=lowDiff;
		}
	}
}


// Motion detection: Uses a three step search algorithm to find an idea motion vector
// Block pattern: (uses 0 as center for simplicity purposes)
// 1 2 3
// 4 0 5
// 6 7 8
static motionVector_t mVectors[9] = { {0, 0}, {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

static uint MotionSearch(uint dimension, uchar *original, motionVector_t *v, int baseX, int baseY)
{
	uchar temp[8*8*3];	// Big enough for anything
	uint blockSize = dimension*dimension*3;
	int dx,dy;
	int offsetX, offsetY;
	int stepSize = 4;	// 3 iterations on a start size of 4 makes +/- 7 pixels, RoQ's limit
	int pick;
	uint lowDiff;
	uint diffs[9];
	int i;

	dx=dy=0;

	while(stepSize)
	{
		for(i=0;i<9;i++)
		{
			if(i==0 && stepSize != 4)
				continue;	// Already got it from the last try

			// Get the diffs
			offsetX = baseX + dx + mVectors[i].x*stepSize;
			offsetY = baseY + dy + mVectors[i].y*stepSize;

			// Don't pick out-of-bounds parts
			if(offsetX < 0 || offsetY < 0 || offsetX+(int)dimension > (int)vidWidth || offsetY+(int)dimension > (int)vidHeight)
			{
				diffs[i] = MAX_MSE;
				continue;
			}

			// Get the real difference
			Blit(rgbFrame1 + (offsetX*3) + (offsetY*vidScanWidth), temp, dimension*3, vidScanWidth, dimension*3, dimension);
			diffs[i] = Diff(temp, original, blockSize);
		}

		// Pick the lowest one
		lowDiff = MAX_MSE;
		for(i=0;i<9;i++)
		{
			if(diffs[i] < lowDiff)
			{
				lowDiff = diffs[i];
				pick = i;
			}
		}

		diffs[0] = diffs[pick];	// Save the difference for the next pass

		// Update the offset
		dx += mVectors[pick].x * stepSize;
		dy += mVectors[pick].y * stepSize;

		stepSize /= 2;
	}

	// Save the result
	v->x = dx;
	v->y = dy;

	return diffs[0];
}


static void MakeMotion8(void)
{
	uint x,y,n;
	uchar comparison[8*8*3];

	n=0;
	for(y=0;y<vidHeight;y+=8)
	{
		for(x=0;x<vidWidth;x+=8)
		{
			Blit(rgbFrame0 + (x + (y*vidWidth))*3, comparison, 8*3, vidScanWidth, 8*3, 8);

			devMotion8[n] = MotionSearch(8, comparison, motionVectors8+n, x, y);
			n++;
		}
	}
}

static void MakeMotion4(void)
{
	uint x,y,n;
	uchar temp[4*4*3];

	n=0;
	for(y=0;y<vidHeight;y+=4)
	{
		for(x=0;x<vidWidth;x+=4)
		{
			Blit(rgbFrame0 + (x*3) + (y*vidScanWidth), temp, 4*3, vidScanWidth, 4*3, 4);
			devMotion4[n] = MotionSearch(4, temp, motionVectors4+n, x, y);

			if(devMotion4[n] < cullThreshold * 4*4)
				optimizedOut4[n] = 1;

			n++;
		}
	}
}

static void MakeSkip(void)
{
	uint x,y,n;
	uchar test1[4*4*3];
	uchar test2[4*4*3];

	n=0;
	for(y=0;y<vidHeight;y+=4)
	{
		for(x=0;x<vidWidth;x+=4)
		{
			Blit(rgbFrame0 + (x*3) + (y*vidScanWidth), test1, 4*3, vidScanWidth, 4*3, 4);
			Blit(rgbFrame2 + (x*3) + (y*vidScanWidth), test2, 4*3, vidScanWidth, 4*3, 4);
			devSkip4[n] = Diff(test1, test2, 4*4*3);

			if(devSkip4[n] < cullThreshold * 4*4)
				optimizedOut4[n] = 1;

			n++;
		}
	}
}

static void RoQEnc_MakeMotionTables(void)
{
	memset(optimizedOut4, 0, sizeof(int) * vidWidth * vidHeight / (4*4));

	// No motion/skip detection for the first 2 frames
	if(comp->frameNum <= 2)
		return;

	// Make MOT and SKIP first so LUT generation can be optimized
	if(comp->framesSinceKeyframe >= 1)
	{
		MakeMotion8();
		MakeMotion4();
	}
	if(comp->framesSinceKeyframe >= 2)
	{
		MakeSkip();
	}
}

static void RoQEnc_MakeDevTables(void)
{
	MakeDevLut8();
	MakeDevLut4();
	MakeDevLut2();
}



// MSE possibility list encoding (Progressive/Brute Force method)
static int possibilityValid[259];
static motionVector_t quadOffsets[4] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
static uint possibilitySizes[259];

static void MakePossibilitySizes(void)
{
	static int done=0;
	uint i;
	uint bit;
	uint size;

	if(done)
		return;		// Only needs to be done once
	done = 1;

	for(i=0;i<3;i++)
		possibilitySizes[i] = bitUsage[i];

	for(i=0;i<256;i++)
	{
		size = SUBQUAD_BIT_USAGE;
		for(bit=0;bit<8;bit+=2)
			size += bitUsage[UNMASK(i, bit)];
		possibilitySizes[i+3] = size;

	}
}

// MakeMSEList :: Makes MSE lists for a given cell
static void MakeMSEList(possibilityList_t *p, uint x, uint y)
{
	uint wo2, wo4, wo8;		// Width Over 2/4/8
	uint n, n2, n4, n8;
	uint mse,i,s,bit;
	uint mask;
	uint offset;

	// Cache offsets of various sizes
	wo2 = vidWidth>>1;
	wo4 = vidWidth>>2;
	wo8 = vidWidth>>3;

	n = x + y*vidWidth;
	n2 = (x>>1) + ((y>>1)*wo2);
	n4 = (x>>2) + ((y>>2)*wo4);
	n8 = (x>>3) + ((y>>3)*wo8);

	p->mse[BLOCKMARK_SKIP] = devSkip4[n4] + devSkip4[n4+1] + devSkip4[n4+wo4] + devSkip4[n4+1+wo4];
	p->mse[BLOCKMARK_MOTION] = devMotion8[n8];
	p->mse[BLOCKMARK_VECTOR] = devLUT8[n8];

	// Sub-entries
	for(i=0;i<256;i++)
	{
		if(!possibilityValid[i])
			continue;	// Don't waste time calculating MSE for bad picks

		mse = 0;
		bit = 0;
		for(s=0;s<4;s++)
		{
			mask = UNMASK(i,bit);
			if(mask == BLOCKMARK_SKIP)
				mse += devSkip4[n4 + quadOffsets[s].x + wo4*quadOffsets[s].y];
			else if(mask == BLOCKMARK_MOTION)
				mse += devMotion4[n4 + quadOffsets[s].x + wo4*quadOffsets[s].y];
			else if(mask == BLOCKMARK_VECTOR)
				mse += devLUT4[n4 + quadOffsets[s].x + wo4*quadOffsets[s].y];
			else if(mask == BLOCKMARK_QUAD)
			{
				// Use 4 LUT2 deviations
				offset = n2 + quadOffsets[s].x*2 + wo2*quadOffsets[s].y*2;
				mse += devLUT2[offset] + devLUT2[offset+1] + devLUT2[offset+wo2] + devLUT2[offset+1+wo2];
			}

			bit+=2;
		}

		p->mse[i+3] = mse;
	}
}

// CalcMSEShift :: Calcualtes the best size-reducing possibility for a given possibility list
// Will pick a possibility as the best if...
// 1.) It is the only one that causes size reduction
// 2.) Its rate is better than all others
// 3.) It ties for best rate, but is smaller
void CalcMSEShift(possibilityList_t *pList)
{
	uint i;
	uchar shiftPossible = 0;
	uint bestLoss;
	ROQDECIMAL bestRate;
	ROQDECIMAL rate;
	ROQDECIMAL amount;
	uint match;
	uint loss;

	bestRate = 0.0;		// Silence compiler warning
	bestLoss = 0;		// Silence compiler warning
	match = 0;			// Silence compiler warning
	for(i=0;i<259;i++)
	{
		if(possibilityValid[i] && (possibilitySizes[i] < pList->currentSize))
		{
			// mse[i] will have LOWER mse than currentMSE....?
			// so amount will be negative...
			// rate = amount / loss   (rate is negative?)
			//
			// Calculate shift rate
			amount = (ROQDECIMAL)pList->mse[i] - (ROQDECIMAL)pList->currentMSE;
			loss = pList->currentSize - possibilitySizes[i];
			rate = amount / (ROQDECIMAL)loss;

			if((!shiftPossible) || rate < bestRate || (rate == bestRate && loss > bestLoss))
			{
				bestRate = rate;
				bestLoss = loss;
				match = i;

				shiftPossible = 1;
			}
		}
	}

	pList->downscalePossible = shiftPossible;
	if(shiftPossible)
	{
		pList->bestDownscaleRate = bestRate;
		pList->bestDownscaleChoice = (ushort)match;
	}
}


// ProgressiveReduceList :: Picks possibilities with the best downshift rate by using
// a pre-sorted list which is updated through bubble sorting

// ComparePList :: Compares two possibility lists, based on validity
// and potential reduction rate
static int ComparePList(const void *pl1, const void *pl2)
{
	possibilityList_t *v1;
	possibilityList_t *v2;
	ROQDECIMAL d1;
	ROQDECIMAL d2;

	v1 = (possibilityList_t *)pl1;
	v2 = (possibilityList_t *)pl2;

	d1 = ((possibilityList_t *)pl1)->bestDownscaleRate;
	d2 = ((possibilityList_t *)pl2)->bestDownscaleRate;

	// Prioritize validity
	if(v1->downscalePossible && (!v2->downscalePossible))
		return -1;
	if(v2->downscalePossible && (!v1->downscalePossible))
		return 1;
	if((!v2->downscalePossible) && (!v1->downscalePossible))
		return 0;

	// Then order
	if(d1 < d2)
		return -1;		// pl1 then pl2
	else if(d2 == d1)
		return 0;
	else
		return 1;
}

// ComparePListEntries :: qsort compare function for two
// integer indexes into the possibility list.  Must be done in
// a separate array because the possibility lists are in the
// order they appear in the image.
static int ComparePListEntries(const void *pl1, const void *pl2)
{
	uint *i1;
	uint *i2;

	i1 = (uint *)pl1;
	i2 = (uint *)pl2;

	return ComparePList(pList + (*i1), pList + (*i2));
}


// ProgressiveReduceList :: Reduces the possibility lists using the best possibilities
// until it fits within the goal bit size, or the possibilities run out.  If there are
// possibilities that will save size while improving or not changing quality (common
// when frames hardly change at all) then it'll do that instead.
static void ProgressiveReduceList(uint goalBits)
{
	uint i;
	uint listCount;
	uint currentSize;
	possibilityList_t *curList;
	uint temp;

	int firstFrame = 1;

	listCount = vidWidth*vidHeight/(8*8);

	// Sort possibility lists by validity and best downshift rate, so that
	// the best optimizations are listed first
	for(i=0;i<listCount;i++)
		listSort[i] = i;


	qsort(listSort, listCount, sizeof(uint), ComparePListEntries);

	currentSize = listCount * possibilitySizes[DEFAULT_PLIST_ENTRY];

	//    Finished compressing       Downscale can be performed for improved quality
	while(currentSize > goalBits || (pList[listSort[0]].bestDownscaleRate <= 0.0 && pList[listSort[0]].downscalePossible))
	{
		curList = pList + listSort[0];
		if(!curList->downscalePossible)
		{
			// Image can't be compressed any further
			break;
		}

		// Subtract the old size
		currentSize -= curList->currentSize;

		// Change the index, MSE, and size
		curList->currentMSE = curList->mse[curList->bestDownscaleChoice];
		curList->currentSize = possibilitySizes[curList->bestDownscaleChoice];
		curList->currentChoice = curList->bestDownscaleChoice;

		// Add the new size
		currentSize += curList->currentSize;

		// Rebuild the compression rating
		CalcMSEShift(curList);

		// Bubble sort it down the list to a new position
		// Doing this with actual data is *WAY* too slow, which is why
		// the sort table is there
		for(i=1;i<listCount;i++)
		{
			// Shift it to the end block if it's valid, or to a new position
			// if it has a worse rate

			if(ComparePList(pList + listSort[i-1], pList + listSort[i]) > 0)
			{
				// Swap the entries
				temp = listSort[i];
				listSort[i] = listSort[i-1];
				listSort[i-1] = temp;
			}
			else
				break;
		}
	}

	firstFrame = 0;
}


void ReconstructImage(void)
{
	uint x,y,bit,sub,n,choice,mask,idx4,idx2;
	uint ox,oy;
	int dx,dy;
	uchar subIndex[4];

	for(n=0;n<256;n++)
		bookentryUsed4[n] = bookentryUsed2[n] = 0;

	// WARNING: If listSort is obsoleted, this needs to be changed
	chosenPossibilities = listSort;
	n=0;
	for(y=0;y<vidHeight;y+=8)
	{
		for(x=0;x<vidWidth;x+=8)
		{
			choice = chosenPossibilities[n] = pList[n].currentChoice;

			if(choice == BLOCKMARK_SKIP)	// Blit from 2 frames ago
				Blit(rgbFrame2 + (x*3) + (y*vidScanWidth), recon + (x*3) + (y*vidScanWidth), 8*3, vidScanWidth, vidScanWidth, 8);
			else if(choice == BLOCKMARK_MOTION)
			{
				// Blit from an offset in the previous frame
				dx = (int)x + motionVectors8[n].x;
				dy = (int)y + motionVectors8[n].y;
				Blit(rgbFrame1 + (dx*3) + (dy*vidScanWidth), recon + (x*3) + (y*vidScanWidth), 8*3, vidScanWidth, vidScanWidth, 8);
			}
			else if(choice == BLOCKMARK_VECTOR)
			{
				// Blit from a codebook entry
				Blit(codebook8[lut8[n]].rgb, recon + (x*3) + (y*vidScanWidth), 8*3, 8*3, vidScanWidth, 8);
				bookentryUsed4[lut8[n]] = 1;
			}
			else
			{
				choice -= 3;

				// Sub-quad encode
				sub = 0;
				for(bit=0;bit<8;bit+=2)
				{
					mask = UNMASK(choice, bit);

					ox = x+quadOffsets[sub].x*4;
					oy = y+quadOffsets[sub].y*4;
					idx4 = (ox>>2) + ((oy*vidWidth)>>4);
					idx2 = (ox>>1) + ((oy*vidWidth)>>2);

					if(mask == BLOCKMARK_SKIP)
						Blit(rgbFrame2+(ox*3) + (oy*vidScanWidth), recon+(ox*3)+(oy*vidScanWidth), 4*3, vidScanWidth, vidScanWidth, 4);
					else if(mask == BLOCKMARK_MOTION)
					{
						// Blit from an offset in the previous frame
						dx = (int)ox + motionVectors4[idx4].x;
						dy = (int)oy + motionVectors4[idx4].y;
						Blit(rgbFrame1 + (dx*3) + (dy*vidScanWidth), recon+(ox*3)+(oy*vidScanWidth), 4*3, vidScanWidth, vidScanWidth, 4);
					}
					else if(mask == BLOCKMARK_VECTOR)
					{
						Blit(codebook4[lut4[idx4]].rgb, recon+(ox*3)+(oy*vidScanWidth), 4*3, 4*3, vidScanWidth, 4);
						bookentryUsed4[lut4[idx4]] = 1;
					}
					else if(mask == BLOCKMARK_QUAD)
					{
						// 4 codebook entries
						subIndex[0] = lut2[idx2];
						subIndex[1] = lut2[idx2+1];
						subIndex[2] = lut2[idx2+(vidWidth>>1)];
						subIndex[3] = lut2[idx2+(vidWidth>>1)+1];

						Blit(codebook2[subIndex[0]].rgb, recon+(ox*3)+(oy*vidScanWidth), 2*3, 2*3, vidScanWidth, 2);
						Blit(codebook2[subIndex[1]].rgb, recon+((ox+2)*3)+(oy*vidScanWidth), 2*3, 2*3, vidScanWidth, 2);
						Blit(codebook2[subIndex[2]].rgb, recon+(ox*3)+((oy+2)*vidScanWidth), 2*3, 2*3, vidScanWidth, 2);
						Blit(codebook2[subIndex[3]].rgb, recon+((ox+2)*3)+((oy+2)*vidScanWidth), 2*3, 2*3, vidScanWidth, 2);

						bookentryUsed2[subIndex[0]] = 1;
						bookentryUsed2[subIndex[1]] = 1;
						bookentryUsed2[subIndex[2]] = 1;
						bookentryUsed2[subIndex[3]] = 1;
					}

					sub++;
				}
			}

			n++;
		}
	}

	// Save the result
	memcpy(rgbFrame0, recon, vidHeight*vidScanWidth);

	stats.reconstructResult = rgbFrame0;
}


static void OptimizeCodebooks(void)
{
	int i,j;

	entriesUsed2 = 0;
	entriesUsed4 = 0;

	// Mark any 2x2 entries used by 4x4 entries, and remap the 4x4s
	for(i=0;i<256;i++)
	{
		if(bookentryUsed4[i])
		{
			for(j=0;j<4;j++)
				bookentryUsed2[yuvDisk4[i].indexes[j]] = 1;

			bookentryRemap4[i] = (uchar)(entriesUsed4++);
		}
	}

	// Remap 2x2 entries
	for(i=0;i<256;i++)
		if(bookentryUsed2[i])
			bookentryRemap2[i] = (uchar)(entriesUsed2++);
}


// Storage control stuff
uchar argumentSpool[64];
int argumentSpoolLength;
ushort typeSpool;
int typeSpoolLength;

uint storeOffset;
uint chunkStart;

void *storeFile;

static void StoreByte(int b) { preWrite[storeOffset++] = (uchar)b; }

static void StoreUShort(ushort s) { StoreByte(s); StoreByte(s>>8); }

static void BeginChunk(ushort mark, ushort parameter)
{
	StoreUShort(mark);
	storeOffset+=4;		// Skip size until it's known
	StoreUShort(parameter);

	chunkStart = storeOffset;
}

static void EndChunk(void)
{
	uint size;

	// Go back and store the size
	size = storeOffset - chunkStart;

	chunkStart -= 6;	// Go back to the size portion

	preWrite[chunkStart++] = (uchar)size;
	preWrite[chunkStart++] = (uchar)(size>>8);
	preWrite[chunkStart++] = (uchar)(size>>16);
	preWrite[chunkStart++] = (uchar)(size>>24);
}

// Video encode spooling
static void FlushSpools(void)
{
	int i;

	// Flush encode types first
	StoreByte(typeSpool & 0xff);
	StoreByte(typeSpool >> 8);

	// Now, flush arguments
	for(i=0;i<argumentSpoolLength;i++)
		StoreByte(argumentSpool[i]);

	argumentSpoolLength = typeSpoolLength = typeSpool = 0;
}

// RoQ parses types in MSB order, then stores them in LSB order.
// Huh?
static void SpoolType(int type)
{
	if(typeSpoolLength == 16)
		FlushSpools();
	typeSpool |= (type & 3) << (14 - typeSpoolLength);

	typeSpoolLength += 2;
}

static void SpoolArgument(uchar argument)
{
	argumentSpool[argumentSpoolLength++] = argument;
}


static void SpoolMotion(motionVector_t *m)
{
	int ax,ay;
	uchar arg;

	ax = 8 - m->x;
	ay = 8 - m->y;

	arg = (uchar)(((ax&15)<<4) | (ay&15));
	SpoolArgument(arg);
}


static void WriteCodebooks(void)
{
	int i,j;
	ushort arg;

	// Store this stuff in the argument
	arg = ((entriesUsed2 & 0xff) << 8) | (entriesUsed4 & 0xff);

	BeginChunk(CHUNKMARK_CODEBOOK, arg);

	// Write 2x2 entries
	for(i=0;i<256;i++)
	{
		if(bookentryUsed2[i])
			for(j=0;j<6;j++)
				StoreByte(yuvCodebook2[i].yuv[j]);
	}

	// Write 4x4 entries
	for(i=0;i<256;i++)
	{
		if(bookentryUsed4[i])
			for(j=0;j<4;j++)
				StoreByte(bookentryRemap2[yuvDisk4[i].indexes[j]]);
	}

	EndChunk();
}

static void StoreCompressedVideo(void)
{
	uint offs2,offs4,offs8;
	uint w2, w4, w8;
	uint x,y;
	uint quad16x, quad16y;
	uint subquad16x, subquad16y;
	uint subquad8x, subquad8y;
	uint choice;
	uint mask;
	uint bit;


	argumentSpoolLength = typeSpoolLength = storeOffset = typeSpool = 0;

	// Store info
	if(!comp->wroteInfo)
	{
		comp->wroteInfo = 1;

		BeginChunk(CHUNKMARK_INFO, 0);
			StoreUShort((ushort)vidWidth);
			StoreUShort((ushort)vidHeight);
			StoreUShort(8);
			StoreUShort(4);
		EndChunk();
	}

	// Write codebooks
	if(entriesUsed2)
		WriteCodebooks();

	// Write video data
	BeginChunk(CHUNKMARK_VIDEO, 0);		// No mean motion

	w2 = vidWidth/2;
	w4 = vidWidth/4;
	w8 = vidWidth/8;

	// Encode in quadtree order
	for(quad16y=0;quad16y<vidHeight;quad16y+=16)
	{
		for(quad16x=0;quad16x<vidWidth;quad16x+=16)
		{
			for(subquad16y=0;subquad16y<16;subquad16y+=8)
			{
				for(subquad16x=0;subquad16x<16;subquad16x+=8)
				{
					// Find the actual coordinate of this cell
					x = quad16x + subquad16x;
					y = quad16y + subquad16y;

					offs2 = (x>>1) + ((y>>1)*w2);
					offs4 = (x>>2) + ((y>>2)*w4);
					offs8 = (x>>3) + ((y>>3)*w8);

					choice = pList[offs8].currentChoice;

					// Encode using the method specified by the current 8x8 macroblock
					if(choice == BLOCKMARK_SKIP)
						SpoolType(BLOCKMARK_SKIP);
					else if(choice == BLOCKMARK_MOTION)
					{
						SpoolType(BLOCKMARK_MOTION);
						SpoolMotion(motionVectors8 + offs8);
					}
					else if(choice == BLOCKMARK_VECTOR)
					{
						SpoolType(BLOCKMARK_VECTOR);
						SpoolArgument(bookentryRemap4[lut8[offs8]]);
					}
					else
					{
						// Encode 4x4 blocks instead
						SpoolType(BLOCKMARK_QUAD);
						choice -= 3;
						bit = 0;
						for(subquad8y=0;subquad8y<2;subquad8y++)
						{
							for(subquad8x=0;subquad8x<2;subquad8x++)
							{
								mask = UNMASK(choice, bit);

								// Encode the sub-block
								if(mask == BLOCKMARK_SKIP)
									SpoolType(BLOCKMARK_SKIP);
								else if(mask == BLOCKMARK_MOTION)
								{
									SpoolType(BLOCKMARK_MOTION);
									SpoolMotion(motionVectors4 + (offs4+subquad8x+subquad8y*w4));
								}
								else if(mask == BLOCKMARK_VECTOR)
								{
									SpoolType(BLOCKMARK_VECTOR);
									SpoolArgument(bookentryRemap4[lut4[offs4+subquad8x+subquad8y*w4]]);
								}
								else if(mask == BLOCKMARK_QUAD)
								{
									SpoolType(BLOCKMARK_QUAD);
									SpoolArgument(bookentryRemap2[lut2[offs2+(subquad8x<<1)+((subquad8y<<1)*w2)]]);
									SpoolArgument(bookentryRemap2[lut2[offs2+(subquad8x<<1)+((subquad8y<<1)*w2)+1]]);
									SpoolArgument(bookentryRemap2[lut2[offs2+(subquad8x<<1)+((subquad8y<<1)*w2)+w2]]);
									SpoolArgument(bookentryRemap2[lut2[offs2+(subquad8x<<1)+((subquad8y<<1)*w2)+1+w2]]);
								}

								bit += 2;
							}
						}
					}
				}
			}
		}
	}

	// Dump any extra data
	if(typeSpoolLength)
		FlushSpools();

	// Finish up
	EndChunk();
}


static void RoQEnc_ProgressiveEncode(void)
{
	uint x,y,n;
	uint bitReserved;
	uint bitAllowed;
	uint mask;

	MakePossibilitySizes();

	// Evaluate possibilities
	for(n=0;n<259;n++)
		possibilityValid[n] = 1;

	if(comp->framesSinceKeyframe < 1 || comp->frameNum <= 2)
		possibilityValid[BLOCKMARK_MOTION] = 0;
	if(comp->framesSinceKeyframe < 2)
		possibilityValid[BLOCKMARK_SKIP] = 0;

	// Determine others
	for(n=0;n<256;n++)
	{
		for(x=0;x<8;x+=2)
		{
			mask = UNMASK(n, x);
			if(mask == BLOCKMARK_MOTION && (comp->framesSinceKeyframe < 1 || comp->frameNum <= 2))
				possibilityValid[n+3] = 0;
			if(mask == BLOCKMARK_SKIP && comp->framesSinceKeyframe < 2)
				possibilityValid[n+3] = 0;
		}
	}

	// Build possibility lists and calculate downshifts
	n=0;
	for(y=0;y<vidHeight;y+=8)
	{
		for(x=0;x<vidWidth;x+=8)
		{
			MakeMSEList(pList+n, x, y);
			pList[n].currentChoice = DEFAULT_PLIST_ENTRY;	// Largest one
			pList[n].currentMSE = pList[n].mse[DEFAULT_PLIST_ENTRY];
			pList[n].currentSize = possibilitySizes[DEFAULT_PLIST_ENTRY];

			CalcMSEShift(pList+n);

			n++;
		}
	}

	// Determine bit allowance
	bitReserved = 0;

	if(!comp->wroteInfo)
		bitReserved += CHUNKHEADERSIZE + INFOBLOCKSIZE;
	bitReserved += CHUNKHEADERSIZE + CBSIZE;

	bitAllowed = comp->preferences[ROQENC_PREF_GOAL_SIZE_BITS];
	if(bitReserved >= bitAllowed)
		bitAllowed = 1;		// Not gonna happen, but at least it won't underflow
	else
		bitAllowed -= bitReserved;

	ProgressiveReduceList(bitAllowed);

	// Recreate the source image, and determine which codebook entries get used
	// in the process.
	ReconstructImage();

	OptimizeCodebooks();

	StoreCompressedVideo();

	// Write everything
	roqi.WriteBuffer(storeFile, preWrite, storeOffset);
}


#define LOCALIZE(x) x = comp->x

ulong RoQEnc_CompressRGB(roq_compressor_t *compressor, void *file, unsigned char *rgb)
{
	videoFrame_t *temp;

	comp = compressor;
	if(!compressor->initialized)
	{
		if(!RoQEnc_InitializeCompressor())
			return 0;
	}

	comp->frameNum++;

	storeFile = file;

	vidWidth = comp->preferences[ROQENC_PREF_WIDTH];
	vidHeight = comp->preferences[ROQENC_PREF_HEIGHT];
	refinementPasses = comp->preferences[ROQENC_PREF_REFINEMENT_PASSES];
	vidScanWidth = vidWidth * 3;

	if(comp->preferences[ROQENC_PREF_KEYFRAME])
		comp->framesSinceKeyframe = 0;
	else
		comp->framesSinceKeyframe++;

	comp->preferences[ROQENC_PREF_KEYFRAME] = 0;	// Automatically break out of keyframe mode

	cullThreshold = comp->preferences[ROQENC_PREF_CULL_THRESHOLD];

	// Cycle frames
	temp = comp->frameCycle[0];
	comp->frameCycle[0] = comp->frameCycle[2];
	comp->frameCycle[2] = comp->frameCycle[1];
	comp->frameCycle[1] = temp;

	// Cache data as globals
	rgbFrame0 = comp->frameCycle[0]->rgb;
	rgbFrame1 = comp->frameCycle[1]->rgb;
	rgbFrame2 = comp->frameCycle[2]->rgb;

	LOCALIZE(devLUT8);     LOCALIZE(devLUT4);     LOCALIZE(devLUT2);
	LOCALIZE(devMotion8);  LOCALIZE(devMotion4);  LOCALIZE(devSkip4);

	LOCALIZE(motionVectors8);
	LOCALIZE(motionVectors4);

	LOCALIZE(lut8);        LOCALIZE(lut4);        LOCALIZE(lut2);
	LOCALIZE(pList);
	LOCALIZE(listSort);

	LOCALIZE(recon);
	LOCALIZE(preWrite);

	LOCALIZE(optimizedOut4);

	// Copy in the image
	memcpy(rgbFrame0, rgb, vidWidth*vidHeight*3);

	RoQEnc_MakeMotionTables();

	// Load or generate codebooks
	if(!RoQEnc_GetCodebooks())
		return 0;

	RoQEnc_MakeDevTables();

	RoQEnc_ProgressiveEncode();

	stats.bytesConsumed = storeOffset;
	stats.bytesMax = comp->preferences[ROQENC_PREF_GOAL_SIZE_BITS] / 8;

	if(stats.bytesConsumed > stats.bytesMax)
		stats.bytesConsumed = stats.bytesMax;

	return storeOffset;
}
