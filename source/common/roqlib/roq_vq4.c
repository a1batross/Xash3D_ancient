//=======================================================================
//			Copyright XashXT Group 2007 ©
//			roq_vq4.c - vq codebook base4
//=======================================================================

#include "roqlib.h"

void YUVPerturb4(yuvBlock4_t *in, yuvBlock4_t *out, uint step)
{
	YUVPerturb2(in->block, out->block, step);
	YUVPerturb2(in->block + 1, out->block + 1, step);
	YUVPerturb2(in->block + 2, out->block + 2, step);
	YUVPerturb2(in->block + 3, out->block + 3, step);
}

uint YUVDifference4(yuvBlock4_t *in, yuvBlock4_t *out)
{
	return YUVDifference2(in->block, out->block) + YUVDifference2(in->block + 1, out->block + 1) + YUVDifference2(in->block + 2, out->block + 2) + YUVDifference2(in->block + 3, out->block + 3);
}

void YUVCentroid4(yuvBlock4_t *blocks, uint count, yuvBlock4_t *out, uint cbSize, uint *map)
{
	uint	numEntries;
	uint	totals[24];
	uint	j, i, k, l, n;
	byte	*yuv;

	// for each entry in the codebook...
	for(i = 0; i < cbSize; i++)
	{
		// initialize the average
		for(j = 0; j < 24; j++) totals[j] = 0;

		numEntries = 0;
		for(j = 0; j < count; j++)
		{
			// if an input element is mapped to this entry...
			if(map[j] == i)
			{
				// increase the number of entries
				numEntries++;

				// and add the values to the total
				for(k = 0, n = 0; k < 4; k++)
				{
					yuv = blocks[j].block[k].yuv;
					for(l = 0; l < 6; l++) totals[n++] += yuv[l];
				}
			}
		}

		// average the results to create a final component
		if(numEntries)
		{
			for(k = 0, n = 0; k < 4; k++)
			{
				yuv = out[i].block[k].yuv;
				for(l = 0; l < 6; l++) yuv[l] = (byte)(totals[n++] / numEntries);
			}
		}
	}
}

#define GLA_FUNCTION_SCOPE
#define GLA_UNIT              yuvBlock4_t
#define GLA_NULL              NULL
#define GLA_DIFFERENCE        YUVDifference4
#define GLA_CENTROID          YUVCentroid4
#define GLA_PRINTF(a, b)	Msg( a, b )
#define GLA_COPY(in,out)      Mem_Copy((out), (in), sizeof(yuvBlock4_t))
#define GLA_PERTURB           YUVPerturb4
#define GLA2_MAX_PASSES       refinementPasses
#define GLA_FUNCTION          YUVGenerateCodebooks4

// YUVGenerateCodebooks4 is DYNAMICALLY CREATED using the following include
#include "roq_vq.h"
