//=======================================================================
//			Copyright XashXT Group 2007 ©
//			roq_vq1.c - vq codebook base1
//=======================================================================

#include "roqlib.h"

// vector quantization routines.  RoQ uses 4:2:0 and 2:1:0 YCbCr blocks for codebooks,
// so I use the same thing, but only because it's about twice as fast as doing RGB
// with very slight quality loss.

yuvBlock4_t yuvCodebook4[256];
yuvBlock2_t yuvCodebook2[256];
yuvDiskBlock4_t yuvDisk4[256]; // Indexed 4x4
static byte codebookBuffer[2560];

static byte ROQ_IndexCodebook2( yuvBlock2_t *block )
{
	int	i;
	uint	diff;
	uint	highestDiff=9999999;
	byte	choice=0;

	// Find the closest match
	for(i = 0; i < 256; i++)
	{
		diff = YUVDifference2(block, yuvCodebook2+i);
		if(diff < highestDiff)
		{
			highestDiff = diff;
			choice = (byte)i;
		}
	}
	return choice;
}

static void ROQ_IndexCodebook4( void )
{
	int i,j;

	for(i = 0; i < 256; i++)
	{
		for(j = 0; j < 4; j++)
			yuvDisk4[i].indexes[j] = ROQ_IndexCodebook2(yuvCodebook4[i].block + j);
	}
}

int ROQ_MakeCodebooks(void)
{
	yuvBlock2_t	*inputs2;
	yuvBlock2_t	*results2;
	yuvBlock4_t	*inputs4;
	yuvBlock4_t	*results4;
	uint		resultCount;
	dword		x,y,n = 0;
	dword		mx, my;
	byte		temp[4*4*3];

	// 2x2 codebook
	inputs2 = RQalloc(sizeof(yuvBlock2_t) * vidWidth * vidHeight / 4);

	for(y = 0; y < vidHeight; y += 2)
	{
		my = (y*vidWidth) / 16;
		for(x = 0; x < vidWidth; x += 2)
		{
			mx = x / 4;
			// check optimization
			if(optimizedOut4[mx+my]) continue;
			ROQ_Blit(rgbFrame0 + (x + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs2[n].yuv);
			n++;
		}
	}

	stats.vectorsProvided4 = vidWidth*vidHeight / 16;
	stats.vectorsQuantized4  = n;
	if(!n) n = 1; // don't feed the generator bad input

	// make it
	if(!YUVGenerateCodebooks2(inputs2, n, 256, &resultCount, &results2))
	{
		Mem_Free(inputs2);
		return 0;
	}

	// save and release the results
	Mem_Copy(yuvCodebook2, results2, sizeof(yuvBlock2_t) * resultCount);
	Mem_Free(results2);
	Mem_Free(inputs2);

	// 4x4 codebook
	inputs4 = RQalloc(sizeof(yuvBlock4_t) * vidWidth * vidHeight / 16);

	for(y = 0, n = 0; y < vidHeight; y += 4)
	{
		my = (y*vidWidth) / 16;
		for(x = 0; x < vidWidth; x += 4)
		{
			mx = x / 4;
			if(optimizedOut4[mx+my]) continue;
			ROQ_Blit(rgbFrame0 + (x + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[0].yuv);
			ROQ_Blit(rgbFrame0 + ((x+2) + (y*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[1].yuv);
			ROQ_Blit(rgbFrame0 + (x + ((y+2)*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[2].yuv);
			ROQ_Blit(rgbFrame0 + ((x+2) + ((y+2)*vidWidth))*3, temp, 2*3, vidScanWidth, 2*3, 2);
			RGB2toYUV2(temp, inputs4[n].block[3].yuv);
			n++;
		}
	}

	if(!n) n = 1; // don't feed the generator bad input
	// make it
	if(!YUVGenerateCodebooks4(inputs4, n, 256, &resultCount, &results4))
	{
		Mem_Free(inputs4);
		return 0;
	}

	// save and release the results
	Mem_Copy(yuvCodebook4, results4, sizeof(yuvBlock4_t) * resultCount);
	Mem_Free(results4);
	Mem_Free(inputs4);
	// index the 4x4 codebook (RoQ requires this)
	ROQ_IndexCodebook4();
	return 1;
}

void GenerateCodebookDebugs(void)
{
	int	x,y,n = 0;

	for(y = 0; y < 16; y++)
	{
		for(x = 0; x < 16; x++)
		{
			// copy a 2x2
			ROQ_Blit(codebook2[n].rgb, stats.codebookResults2 + (x*2*3) + (y*2*(2*16*3)), 2*3, 2*3, 16*2*3, 2);
			ROQ_Blit(codebook4[n].rgb, stats.codebookResults4 + (x*4*3) + (y*4*(4*16*3)), 4*3, 4*3, 16*4*3, 4);
			n++;
		}
	}
}

int ROQ_GetCodebooks(void)
{
	int	i;

	if(!ROQ_MakeCodebooks())
	{
		// generate codebooks for this frame
		return 0;
	}
	else
	{
		stats.vectorsProvided4 = vidWidth * vidHeight / 16;
		stats.vectorsQuantized4  = stats.vectorsProvided4;
	}

	// decode the 2x2 codebook
	for(i = 0; i < 256; i++) YUV2toRGB2(yuvCodebook2[i].yuv, codebook2[i].rgb);
	// copy the 2x2 codebook to the 4x4 codebook using the indices
	for(i = 0; i < 256; i++)
	{
		ROQ_Blit(codebook2[yuvDisk4[i].indexes[0]].rgb, codebook4[i].rgb, 2*3, 2*3, 4*3, 2);
		ROQ_Blit(codebook2[yuvDisk4[i].indexes[1]].rgb, codebook4[i].rgb+6, 2*3, 2*3, 4*3, 2);
		ROQ_Blit(codebook2[yuvDisk4[i].indexes[2]].rgb, codebook4[i].rgb+24, 2*3, 2*3, 4*3, 2);
		ROQ_Blit(codebook2[yuvDisk4[i].indexes[3]].rgb, codebook4[i].rgb+24+6, 2*3, 2*3, 4*3, 2);
	}

	// create 8x8 codebooks by oversizing the 4x4 entries
	for(i = 0; i < 256; i++) ROQ_DoubleSize(codebook4[i].rgb, codebook8[i].rgb, 4);
	GenerateCodebookDebugs();

	return 1;
}
