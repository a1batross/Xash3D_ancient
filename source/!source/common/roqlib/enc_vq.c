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

// Vector quantization routines.  RoQ uses 4:2:0 and 2:1:0 YCbCr blocks for codebooks,
// so I use the same thing, but only because it's about twice as fast as doing RGB
// with very slight quality loss.

yuvBlock4_t yuvCodebook4[256];
yuvBlock2_t yuvCodebook2[256];

yuvDiskBlock4_t yuvDisk4[256];	// Indexed 4x4

static uchar RoQEnc_IndexCodebook2(yuvBlock2_t *block)
{
	int i;
	uint diff;
	uint highestDiff=9999999;
	uchar choice=0;

	// Find the closest match
	for(i=0;i<256;i++)
	{
		diff = YUVDifference2(block, yuvCodebook2+i);
		if(diff < highestDiff)
		{
			highestDiff = diff;
			choice = (uchar)i;
		}
	}

	return choice;
}

static void RoQEnc_IndexCodebook4(void)
{
	int i,j;

	for(i=0;i<256;i++)
	{
		for(j=0;j<4;j++)
			yuvDisk4[i].indexes[j] = RoQEnc_IndexCodebook2(yuvCodebook4[i].block + j);
	}
}

int RoQEnc_MakeCodebooks(void)
{
	yuvBlock2_t *inputs2;
	yuvBlock2_t *results2;
	yuvBlock4_t *inputs4;
	yuvBlock4_t *results4;
	uint resultCount;
	ulong x,y,n;
	ulong mx, my;
	ulong offs;

	uchar temp[4*4*3];

	// 2x2 codebook
	inputs2 = malloc(sizeof(yuvBlock2_t) * vidWidth * vidHeight / 4);
	if(!inputs2)
		return 0;

	n=0;
	for(y=0;y<vidHeight;y+=2)
	{
		my = (y*vidWidth) / 16;

		for(x=0;x<vidWidth;x+=2)
		{
			mx = x / 4;

			// Check optimization
			if(optimizedOut4[mx+my])
				continue;

			Blit(rgbFrame0 + (x + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);

			RGB2toYUV2(temp, inputs2[n].yuv);
			n++;
		}
	}

	stats.vectorsProvided4 = vidWidth*vidHeight / 16;
	stats.vectorsQuantized4  = n;


	if(!n) n = 1;	// Don't feed the generator bad input

	// Make it
	if(!YUVGenerateCodebooks2(inputs2, n, 256, &resultCount, &results2))
	{
		free(inputs2);
		return 0;
	}

	// Save and release the results
	memcpy(yuvCodebook2, results2, sizeof(yuvBlock2_t) * resultCount);
	free(results2);
	free(inputs2);


	// 4x4 codebook
	inputs4 = malloc(sizeof(yuvBlock4_t) * vidWidth * vidHeight / 16);

	n=0;
	for(y=0;y<vidHeight;y+=4)
	{
		my = (y*vidWidth) / 16;

		for(x=0;x<vidWidth;x+=4)
		{
			mx = x / 4;

			if(optimizedOut4[mx+my])
				continue;

			Blit(rgbFrame0 + (x + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[0].yuv);

			Blit(rgbFrame0 + ((x+2) + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[1].yuv);

			Blit(rgbFrame0 + (x + ((y+2)*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[2].yuv);

			Blit(rgbFrame0 + ((x+2) + ((y+2)*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[3].yuv);

			n++;
		}
	}

	if(!n) n = 1;	// Don't feed the generator bad input

	// Make it
	if(!YUVGenerateCodebooks4(inputs4, n, 256, &resultCount, &results4))
	{
		free(inputs4);
		return 0;
	}

	// Save and release the results
	memcpy(yuvCodebook4, results4, sizeof(yuvBlock4_t) * resultCount);
	free(results4);
	free(inputs4);

	// Index the 4x4 codebook (RoQ requires this)
	RoQEnc_IndexCodebook4();

	return 1;
}


void GenerateCodebookDebugs(void)
{
	int x,y,n;

	n = 0;
	for(y=0;y<16;y++)
	{
		for(x=0;x<16;x++)
		{
			// Copy a 2x2
			Blit(codebook2[n].rgb, stats.codebookResults2 + (x*2*3) + (y*2*(2*16*3)), 2*3, 2*3, 16*2*3, 2);
			Blit(codebook4[n].rgb, stats.codebookResults4 + (x*4*3) + (y*4*(4*16*3)), 4*3, 4*3, 16*4*3, 4);
			n++;
		}
	}
}

static uchar codebookBuffer[2560];

int ReadCodebookCache(void)
{
	uint i,j,n;

	if(!roqi.ReadCodebook)
		return 0;
	if(!roqi.ReadCodebook(codebookBuffer))
		return 0;

	n=0;
	// Parse the codebook data
	for(i=0;i<256;i++)
		for(j=0;j<6;j++)
			yuvCodebook2[i].yuv[j] = codebookBuffer[n++];

	for(i=0;i<256;i++)
		for(j=0;j<4;j++)
			yuvDisk4[i].indexes[j] = codebookBuffer[n++];

	return 1;
}

void WriteCodebookCache(void)
{
	uint i,j,n;

	if(!roqi.SaveCodebook)
		return;

	n=0;
	for(i=0;i<256;i++)
		for(j=0;j<6;j++)
			codebookBuffer[n++] = yuvCodebook2[i].yuv[j];

	for(i=0;i<256;i++)
		for(j=0;j<4;j++)
			codebookBuffer[n++] = yuvDisk4[i].indexes[j];

	roqi.SaveCodebook(codebookBuffer);
}


int RoQEnc_GetCodebooks(void)
{
	int i;
	int dontWrite;

	if(!ReadCodebookCache())
	{
		// Generate codebooks for this frame
		if(!RoQEnc_MakeCodebooks())
			return 0;
		dontWrite = 0;
	}
	else
	{
		stats.vectorsProvided4 = vidWidth*vidHeight / 16;
		stats.vectorsQuantized4  = stats.vectorsProvided4;
		dontWrite = 1;
	}

	// Decode the 2x2 codebook
	for(i=0;i<256;i++)
		YUV2toRGB2(yuvCodebook2[i].yuv, codebook2[i].rgb);

	// Copy the 2x2 codebook to the 4x4 codebook using the indices
	for(i=0;i<256;i++)
	{
		Blit(codebook2[yuvDisk4[i].indexes[0]].rgb, codebook4[i].rgb, 2*3, 2*3, 4*3, 2);
		Blit(codebook2[yuvDisk4[i].indexes[1]].rgb, codebook4[i].rgb+6, 2*3, 2*3, 4*3, 2);
		Blit(codebook2[yuvDisk4[i].indexes[2]].rgb, codebook4[i].rgb+24, 2*3, 2*3, 4*3, 2);
		Blit(codebook2[yuvDisk4[i].indexes[3]].rgb, codebook4[i].rgb+24+6, 2*3, 2*3, 4*3, 2);
	}

	// Create 8x8 codebooks by oversizing the 4x4 entries
	for(i=0;i<256;i++)
		DoubleSize(codebook4[i].rgb, codebook8[i].rgb, 4);

	GenerateCodebookDebugs();

	if(!dontWrite)
		WriteCodebookCache();

	return 1;
}
