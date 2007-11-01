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

#include "roqe_local.h"

extern yuvBlock2_t yuvCodebook2[256];

static void YUVCentroid2(yuvBlock2_t *inputs, uint count, yuvBlock2_t *out, uint cbSize, uint *map)
{
	unsigned int numEntries;
	unsigned int totals[6];
	unsigned int i;
	unsigned int j;
	unsigned int k;
	uchar *yuv;

	// For each entry in the codebook...
	for(i=0;i<cbSize;i++)
	{
		// Initialize the average
		totals[0] = totals[1] = totals[2] = totals[3] = totals[4] = totals[5] = 0;
		numEntries = 0;
		for(j=0;j<count;j++)
		{
			// If an input element is mapped to this entry...
			if(map[j] == i)
			{
				// Increase the number of entries
				numEntries++;
				// And add the values to the total
				yuv = inputs[j].yuv;
				for(k=0;k<6;k++)
					totals[k] += yuv[k];
			}
		}

		// Average the results to create a final component
		if(numEntries)
		{
			yuv = out[i].yuv;
			for(k=0;k<6;k++)
				yuv[k] = (uchar)(totals[k] / numEntries);
		}
	}
}

uint YUVDifference2(yuvBlock2_t *a, yuvBlock2_t *b)
{
	uchar *yuv1;
	uchar *yuv2;

	yuv1 = a->yuv;
	yuv2 = b->yuv;

	return QUICKDIFF(yuv1[0],yuv2[0]) + QUICKDIFF(yuv1[1],yuv2[1]) + \
		QUICKDIFF(yuv1[2],yuv2[2]) + QUICKDIFF(yuv1[3],yuv2[3]) + \
		(QUICKDIFF(yuv1[4],yuv2[4]) << 2) + (QUICKDIFF(yuv1[5],yuv2[5]) << 2);
}

void YUVPerturb2(yuvBlock2_t *a, yuvBlock2_t *b, uint step)
{
	int diff;
	int res;
	int i;

	static unsigned int lastStep = 999;
	static int rmax;
	
	diff = 0;		// Silence compiler warning

	if(lastStep != step)
	{
		lastStep = step;
		rmax = 1;
		for(i=0;i<PERTURBATION_BASE_POWER - (int)step;i++)
			rmax <<= 1;
	}

	if(rmax < 2)
		rmax = 2;

	for(i=0;i<6;i++)
	{
		if(rmax)
			diff = rmax;
		else
			diff = 0;
		res = a->yuv[i] + diff;
		if(res < 0)
			res = 0;
		if(res > 255)
			res = 255;
		b->yuv[i] = (uchar)res;

		res = a->yuv[i] - diff;
		if(res < 0)
			res = 0;
		if(res > 255)
			res = 255;

		a->yuv[i] = (uchar)res;

		if(i == 4)
			rmax /= 2;
	}
}


#define GLA_FUNCTION_SCOPE
#define GLA_UNIT              yuvBlock2_t
#define GLA_NULL              NULL
#define GLA_DIFFERENCE        YUVDifference2
#define GLA_CENTROID          YUVCentroid2
#define GLA_PRINTF(a,b)
#define GLA_COPY(in,out)      memcpy((out), (in), sizeof(yuvBlock2_t))
#define GLA_PERTURB           YUVPerturb2
#define GLA2_MAX_PASSES       refinementPasses
#define GLA_FUNCTION          YUVGenerateCodebooks2

// YUVGenerateCodebooks2 is DYNAMICALLY CREATED using the following include
#include "stdvq.ci"
