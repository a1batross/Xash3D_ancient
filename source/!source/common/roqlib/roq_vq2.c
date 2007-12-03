//=======================================================================
//			Copyright XashXT Group 2007 ©
//			roq_vq2.c - vq codebook base2
//=======================================================================

#include "roqlib.h"

static void YUVCentroid2(yuvBlock2_t *inputs, uint count, yuvBlock2_t *out, uint cbSize, uint *map)
{
	uint	numEntries;
	uint	totals[6];
	uint	i, j, k;
	byte	*yuv;

	// for each entry in the codebook...
	for(i = 0; i < cbSize; i++)
	{
		// initialize the average
		totals[0] = totals[1] = totals[2] = totals[3] = totals[4] = totals[5] = 0;
		numEntries = 0;
		for(j = 0; j < count; j++)
		{
			// if an input element is mapped to this entry...
			if(map[j] == i)
			{
				// increase the number of entries
				numEntries++;
				// and add the values to the total
				yuv = inputs[j].yuv;
				for(k = 0; k < 6; k++) totals[k] += yuv[k];
			}
		}

		// average the results to create a final component
		if(numEntries)
		{
			yuv = out[i].yuv;
			for(k = 0; k < 6; k++) yuv[k] = (byte)(totals[k] / numEntries);
		}
	}
}

uint YUVDifference2(yuvBlock2_t *a, yuvBlock2_t *b)
{
	byte *yuv1, *yuv2;

	yuv1 = a->yuv;
	yuv2 = b->yuv;

	return QUICKDIFF(yuv1[0],yuv2[0]) + QUICKDIFF(yuv1[1],yuv2[1]) + QUICKDIFF(yuv1[2],yuv2[2]) + QUICKDIFF(yuv1[3],yuv2[3]) + (QUICKDIFF(yuv1[4],yuv2[4]) << 2) + (QUICKDIFF(yuv1[5],yuv2[5]) << 2);
}

void YUVPerturb2(yuvBlock2_t *a, yuvBlock2_t *b, uint step)
{
	long		i, res, diff = 0;
	static uint	lastStep = 999;
	static long	rmax;
	
	if(lastStep != step)
	{
		lastStep = step;
		rmax = 1;
		for(i = 0; i < PERTURBATION_BASE_POWER - (int)step; i++) rmax <<= 1;
	}

	if(rmax < 2) rmax = 2;
	for(i = 0; i < 6; i++)
	{
		if(rmax) diff = rmax;
		else diff = 0;
		res = a->yuv[i] + diff;
		if(res < 0) res = 0;
		if(res > 255) res = 255;
		b->yuv[i] = (byte)res;

		res = a->yuv[i] - diff;
		if(res < 0) res = 0;
		if(res > 255) res = 255;
		a->yuv[i] = (byte)res;
		if(i == 4) rmax /= 2;
	}
}


#define GLA_FUNCTION_SCOPE
#define GLA_UNIT              yuvBlock2_t
#define GLA_NULL              NULL
#define GLA_DIFFERENCE        YUVDifference2
#define GLA_CENTROID          YUVCentroid2
#define GLA_PRINTF(a,b)       Msg( a, b )
#define GLA_COPY(in,out)      Mem_Copy((out), (in), sizeof(yuvBlock2_t))
#define GLA_PERTURB           YUVPerturb2
#define GLA2_MAX_PASSES       refinementPasses
#define GLA_FUNCTION          YUVGenerateCodebooks2

// YUVGenerateCodebooks2 is DYNAMICALLY CREATED using the following include
#include "roq_vq.h"
