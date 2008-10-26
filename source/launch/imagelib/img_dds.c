//=======================================================================
//			Copyright XashXT Group 2008 ©
//			img_dds.c - dds format load & save
//=======================================================================

#include "imagelib.h"
#include "mathlib.h"

// TODO: tune this ?
#define REDWEIGHT		4
#define GREENWEIGHT		16
#define BLUEWEIGHT		1

#define CHAN_MAX		255
#define ALPHACUT		127

static void Image_BaseColorSearch( byte *blkaddr, byte srccolors[4][4][4], byte *bestcolor[2], int numxpixels, int numypixels, int type, bool haveAlpha )
{
	// use same luminance-weighted distance metric to determine encoding as for finding the base colors

	// TODO: could also try to find a better encoding for the 3-color-encoding type, this really should be done
	// if it's rgba_dxt1 and we have alpha in the block, currently even values which will be mapped to black
	// due to their alpha value will influence the result
	int	i, j, colors, z;
	uint	pixerror, pixerrorred, pixerrorgreen, pixerrorblue, pixerrorbest;
	int	colordist, blockerrlin[2][3];
	byte	nrcolor[2];
	int	pixerrorcolorbest[3];
	byte	enc = 0;
	byte	cv[4][4];
	byte	testcolor[2][3];

	if(((bestcolor[0][0] & 0xf8) << 8 | (bestcolor[0][1] & 0xfc) << 3 | bestcolor[0][2] >> 3) < ((bestcolor[1][0] & 0xf8) << 8 | (bestcolor[1][1] & 0xfc) << 3 | bestcolor[1][2] >> 3))
	{
		testcolor[0][0] = bestcolor[0][0];
		testcolor[0][1] = bestcolor[0][1];
		testcolor[0][2] = bestcolor[0][2];
		testcolor[1][0] = bestcolor[1][0];
		testcolor[1][1] = bestcolor[1][1];
		testcolor[1][2] = bestcolor[1][2];
	}
	else
	{
		testcolor[1][0] = bestcolor[0][0];
		testcolor[1][1] = bestcolor[0][1];
		testcolor[1][2] = bestcolor[0][2];
		testcolor[0][0] = bestcolor[1][0];
		testcolor[0][1] = bestcolor[1][1];
		testcolor[0][2] = bestcolor[1][2];
	}

	for( i = 0; i < 3; i ++ )
	{
		cv[0][i] = testcolor[0][i];
		cv[1][i] = testcolor[1][i];
		cv[2][i] = (testcolor[0][i] * 2 + testcolor[1][i]) / 3;
		cv[3][i] = (testcolor[0][i] + testcolor[1][i] * 2) / 3;
	}

	blockerrlin[0][0] = 0;
	blockerrlin[0][1] = 0;
	blockerrlin[0][2] = 0;
	blockerrlin[1][0] = 0;
	blockerrlin[1][1] = 0;
	blockerrlin[1][2] = 0;

	nrcolor[0] = 0;
	nrcolor[1] = 0;

	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			pixerrorbest = 0xffffffff;
			for( colors = 0; colors < 4; colors++ )
			{
				colordist = srccolors[j][i][0] - (cv[colors][0]);
				pixerror = colordist * colordist * REDWEIGHT;
				pixerrorred = colordist;
				colordist = srccolors[j][i][1] - (cv[colors][1]);
				pixerror += colordist * colordist * GREENWEIGHT;
				pixerrorgreen = colordist;
				colordist = srccolors[j][i][2] - (cv[colors][2]);
				pixerror += colordist * colordist * BLUEWEIGHT;
				pixerrorblue = colordist;
				if( pixerror < pixerrorbest )
				{
					enc = colors;
					pixerrorbest = pixerror;
					pixerrorcolorbest[0] = pixerrorred;
					pixerrorcolorbest[1] = pixerrorgreen;
					pixerrorcolorbest[2] = pixerrorblue;
				}
			}
			if( enc == 0 )
			{
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[0][z] += 3 * pixerrorcolorbest[z];
				}
				nrcolor[0] += 3;
			}
			else if( enc == 2 )
			{
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[0][z] += 2 * pixerrorcolorbest[z];
				}
				nrcolor[0] += 2;
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[1][z] += 1 * pixerrorcolorbest[z];
				}
				nrcolor[1] += 1;
			}
			else if( enc == 3 )
			{
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[0][z] += 1 * pixerrorcolorbest[z];
				}
				nrcolor[0] += 1;
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[1][z] += 2 * pixerrorcolorbest[z];
				}
				nrcolor[1] += 2;
			}
			else if( enc == 1 )
			{
				for( z = 0; z < 3; z++ )
				{
					blockerrlin[1][z] += 3 * pixerrorcolorbest[z];
				}
				nrcolor[1] += 3;
			}
		}
	}

	if( nrcolor[0] == 0 ) nrcolor[0] = 1;
	if( nrcolor[1] == 0 ) nrcolor[1] = 1;
	for( j = 0; j < 2; j++ )
	{
		for( i = 0; i < 3; i++ )
		{
			int newvalue = testcolor[j][i] + blockerrlin[j][i] / nrcolor[j];
			if( newvalue <= 0 ) testcolor[j][i] = 0;
			else if( newvalue >= 255 ) testcolor[j][i] = 255;
			else testcolor[j][i] = newvalue;
		}
	}

	if((abs(testcolor[0][0] - testcolor[1][0]) < 8) && (abs(testcolor[0][1] - testcolor[1][1]) < 4) && (abs(testcolor[0][2] - testcolor[1][2]) < 8))
	{
		// both colors are so close they might get encoded as the same 16bit values
		byte	coldiffred, coldiffgreen, coldiffblue, coldiffmax, factor, ind0, ind1;

		coldiffred = abs(testcolor[0][0] - testcolor[1][0]);
		coldiffgreen = 2 * abs(testcolor[0][1] - testcolor[1][1]);
		coldiffblue = abs(testcolor[0][2] - testcolor[1][2]);
		coldiffmax = coldiffred;

		if( coldiffmax < coldiffgreen ) coldiffmax = coldiffgreen;
		if( coldiffmax < coldiffblue ) coldiffmax = coldiffblue;
		if( coldiffmax > 0 )
		{
			if( coldiffmax > 4 ) factor = 2;
			else if( coldiffmax > 2 ) factor = 3;
			else factor = 4;

			// won't do much if the color value is near 255...
			// argh so many ifs
			if( testcolor[1][1] >= testcolor[0][1] )
			{
				ind1 = 1;
				ind0 = 0;
			}
			else
			{
				ind1 = 0;
				ind0 = 1;
			}

			if((testcolor[ind1][1] + factor * coldiffgreen) <= 255 )
				testcolor[ind1][1] += factor * coldiffgreen;
			else testcolor[ind1][1] = 255;

			if((testcolor[ind1][0] - testcolor[ind0][1]) > 0 )
			{
				if((testcolor[ind1][0] + factor * coldiffred) <= 255 )
					testcolor[ind1][0] += factor * coldiffred;
				else testcolor[ind1][0] = 255;
			}
			else
			{
				if((testcolor[ind0][0] + factor * coldiffred) <= 255 )
					testcolor[ind0][0] += factor * coldiffred;
				else testcolor[ind0][0] = 255;
			}

			if((testcolor[ind1][2] - testcolor[ind0][2]) > 0 )
			{
				if((testcolor[ind1][2] + factor * coldiffblue) <= 255 )
					testcolor[ind1][2] += factor * coldiffblue;
				else testcolor[ind1][2] = 255;
			}
			else
			{
				if((testcolor[ind0][2] + factor * coldiffblue) <= 255 )
					testcolor[ind0][2] += factor * coldiffblue;
				else testcolor[ind0][2] = 255;
			}
		}
	}

	if(((testcolor[0][0] & 0xf8) << 8 | (testcolor[0][1] & 0xfc) << 3 | testcolor[0][2] >> 3) < ((testcolor[1][0] & 0xf8) << 8 | (testcolor[1][1] & 0xfc) << 3 | testcolor[1][2]) >> 3)
	{
		for( i = 0; i < 3; i++ )
		{
			bestcolor[0][i] = testcolor[0][i];
			bestcolor[1][i] = testcolor[1][i];
		}
	}
	else
	{
		for( i = 0; i < 3; i++ )
		{
			bestcolor[0][i] = testcolor[1][i];
			bestcolor[1][i] = testcolor[0][i];
		}
	}
}

static void Image_StoreBlock( byte *blkaddr, byte srccolors[4][4][4], byte *bestcolor[2], int numxpixels, int numypixels, uint type, bool haveAlpha )
{
	// use same luminance-weighted distance metric to determine encoding as for finding the base colors
	int	i, j, colors;
	uint	testerror, testerror2, pixerror, pixerrorbest;
	int	colordist;
	word	color0, color1, tempcolor;
	uint	bits = 0, bits2 = 0;
	byte	*colorptr;
	byte	enc = 0;
	byte	cv[4][4];

	bestcolor[0][0] = bestcolor[0][0] & 0xf8;
	bestcolor[0][1] = bestcolor[0][1] & 0xfc;
	bestcolor[0][2] = bestcolor[0][2] & 0xf8;
	bestcolor[1][0] = bestcolor[1][0] & 0xf8;
	bestcolor[1][1] = bestcolor[1][1] & 0xfc;
	bestcolor[1][2] = bestcolor[1][2] & 0xf8;

	color0 = bestcolor[0][0] << 8 | bestcolor[0][1] << 3 | bestcolor[0][2] >> 3;
	color1 = bestcolor[1][0] << 8 | bestcolor[1][1] << 3 | bestcolor[1][2] >> 3;

	if( color0 < color1 )
	{
		tempcolor = color0;
		color0 = color1;
		color1 = tempcolor;
		colorptr = bestcolor[0];
		bestcolor[0] = bestcolor[1];
		bestcolor[1] = colorptr;
	}

	for( i = 0; i < 3; i++ )
	{
		cv[0][i] = bestcolor[0][i];
		cv[1][i] = bestcolor[1][i];
		cv[2][i] = (bestcolor[0][i] * 2 + bestcolor[1][i]) / 3;
		cv[3][i] = (bestcolor[0][i] + bestcolor[1][i] * 2) / 3;
	}

	testerror = 0;
	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			pixerrorbest = 0xffffffff;

			for( colors = 0; colors < 4; colors++ )
			{
				colordist = srccolors[j][i][0] - cv[colors][0];
				pixerror = colordist * colordist * REDWEIGHT;
				colordist = srccolors[j][i][1] - cv[colors][1];
				pixerror += colordist * colordist * GREENWEIGHT;
				colordist = srccolors[j][i][2] - cv[colors][2];
				pixerror += colordist * colordist * BLUEWEIGHT;

				if( pixerror < pixerrorbest )
				{
					pixerrorbest = pixerror;
					enc = colors;
				}
			}
			testerror += pixerrorbest;
			bits |= enc << (2 * (j * 4 + i));
		}
	}

	for( i = 0; i < 3; i++ )
	{
		cv[2][i] = (bestcolor[0][i] + bestcolor[1][i]) / 2;

		// this isn't used. Looks like the black color constant can only be used
		// with RGB_DXT1 if I read the spec correctly (note though that the radeon gpu disagrees,
		// it will decode 3 to black even with DXT3/5), and due to how the color searching works
		// it won't get used even then
		cv[3][i] = 0;
	}

	testerror2 = 0;
	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			pixerrorbest = 0xffffffff;
			if((type == PF_DXT1) && (srccolors[j][i][3] <= ALPHACUT))
			{
				enc = 3;
				pixerrorbest = 0; // don't calculate error
			}
			else
			{
				// we're calculating the same what we have done already for colors 0-1 above...
				for( colors = 0; colors < 3; colors++ )
				{
					colordist = srccolors[j][i][0] - cv[colors][0];
					pixerror = colordist * colordist * REDWEIGHT;
					colordist = srccolors[j][i][1] - cv[colors][1];
					pixerror += colordist * colordist * GREENWEIGHT;
					colordist = srccolors[j][i][2] - cv[colors][2];
					pixerror += colordist * colordist * BLUEWEIGHT;

					if( pixerror < pixerrorbest )
					{
						pixerrorbest = pixerror;

						// need to exchange colors later
						if( colors > 1 ) enc = colors;
						else enc = colors ^ 1;
					}
				}
			}
			testerror2 += pixerrorbest;
			bits2 |= enc << (2 * (j * 4 + i));
		}
	}

	// finally we're finished, write back colors and bits
	if((testerror > testerror2) || (haveAlpha))
	{
		*blkaddr++ = color1 & 0xFF;
		*blkaddr++ = color1 >> 8;
		*blkaddr++ = color0 & 0xFF;
		*blkaddr++ = color0 >> 8;
		*blkaddr++ = bits2 & 0xFF;
		*blkaddr++ = (bits2 >> 8) & 0xFF;
		*blkaddr++ = (bits2 >> 16) & 0xFF;
		*blkaddr = bits2 >> 24;
	}
	else
	{
		*blkaddr++ = color0 & 0xFF;
		*blkaddr++ = color0 >> 8;
		*blkaddr++ = color1 & 0xFF;
		*blkaddr++ = color1 >> 8;
		*blkaddr++ = bits & 0xFF;
		*blkaddr++ = ( bits >> 8) & 0xFF;
		*blkaddr++ = ( bits >> 16) & 0xFF;
		*blkaddr = bits >> 24;
	}
}

static void Image_EncodeColorBlock( byte *blkaddr, byte srccolors[4][4][4], int numxpixels, int numypixels, uint type, int flags )
{
	// simplistic approach. We need two base colors, simply use the "highest" and the "lowest" color
	// present in the picture as base colors

	// define lowest and highest color as shortest and longest vector to 0/0/0, though the
	// vectors are weighted similar to their importance in rgb-luminance conversion
	// doesn't work too well though...
	// this seems to be a rather difficult problem
	byte	*bestcolor[2];
	byte	basecolors[2][3];
	uint	lowcv, highcv, testcv;
	bool	haveAlpha = false;
	byte	i, j;

	lowcv = highcv = srccolors[0][0][0] * srccolors[0][0][0] * REDWEIGHT + srccolors[0][0][1] * srccolors[0][0][1] * GREENWEIGHT + srccolors[0][0][2] * srccolors[0][0][2] * BLUEWEIGHT;
	bestcolor[0] = bestcolor[1] = srccolors[0][0];

	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			// don't use this as a base color if the pixel will get black/transparent anyway
			if((type != PF_DXT1) || (srccolors[j][i][3] <= ALPHACUT))
			{
				testcv = srccolors[j][i][0] * srccolors[j][i][0] * REDWEIGHT + srccolors[j][i][1] * srccolors[j][i][1] * GREENWEIGHT + srccolors[j][i][2] * srccolors[j][i][2] * BLUEWEIGHT;
				if( testcv > highcv )
				{
					highcv = testcv;
					bestcolor[1] = srccolors[j][i];
				}
				else if( testcv < lowcv )
				{
					lowcv = testcv;
					bestcolor[0] = srccolors[j][i];
				}
			}
			else haveAlpha = true;
		}
	}

	if( type != PF_DXT1 )
	{	
		// manually set alpha for DXT3 or DXT5
		if( flags & IMAGE_HAVE_ALPHA ) haveAlpha = true;
		else haveAlpha = false;
	}

	// make sure the original color values won't get touched...
	for( j = 0; j < 2; j++ )
	{
		for( i = 0; i < 3; i++ )
		{
			basecolors[j][i] = bestcolor[j][i];
		}
	}

	bestcolor[0] = basecolors[0];
	bestcolor[1] = basecolors[1];

	// try to find better base colors
	Image_BaseColorSearch( blkaddr, srccolors, bestcolor, numxpixels, numypixels, type, haveAlpha );
	// find the best encoding for these colors, and store the result
	Image_StoreBlock( blkaddr, srccolors, bestcolor, numxpixels, numypixels, type, haveAlpha );
}

static void Image_StoreAlphaBlock( byte *blkaddr, byte alphabase1, byte alphabase2, byte alphaenc[16] )
{
	*blkaddr++ = alphabase1;
	*blkaddr++ = alphabase2;
	*blkaddr++ = alphaenc[0] | (alphaenc[1] << 3) | ((alphaenc[2] & 3) << 6);
	*blkaddr++ = (alphaenc[2] >> 2) | (alphaenc[3] << 1) | (alphaenc[4] << 4) | ((alphaenc[5] & 1) << 7);
	*blkaddr++ = (alphaenc[5] >> 1) | (alphaenc[6] << 2) | (alphaenc[7] << 5);
	*blkaddr++ = alphaenc[8] | (alphaenc[9] << 3) | ((alphaenc[10] & 3) << 6);
	*blkaddr++ = (alphaenc[10] >> 2) | (alphaenc[11] << 1) | (alphaenc[12] << 4) | ((alphaenc[13] & 1) << 7);
	*blkaddr++ = (alphaenc[13] >> 1) | (alphaenc[14] << 2) | (alphaenc[15] << 5);
}

static void Image_EncodeDXT5alpha( byte *blkaddr, byte srccolors[4][4][4], int numxpixels, int numypixels )
{
	byte	alphabase[2], alphause[2];
	short	alphatest[2];
	uint	alphablockerror1, alphablockerror2, alphablockerror3;
	byte	i, j, aindex, acutValues[7];
	byte	alphaenc1[16], alphaenc2[16], alphaenc3[16];
	bool	alphaabsmin = false;
	bool	alphaabsmax = false;
	short	alphadist;

	// find lowest and highest alpha value in block, alphabase[0] lowest, alphabase[1] highest
	alphabase[0] = 0xFF;
	alphabase[1] = 0x00;

	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			if( srccolors[j][i][3] == 0 )
				alphaabsmin = true;
			else if( srccolors[j][i][3] == 255 )
				alphaabsmax = true;
			else
			{
				if( srccolors[j][i][3] > alphabase[1] )
					alphabase[1] = srccolors[j][i][3];
				if( srccolors[j][i][3] < alphabase[0] )
					alphabase[0] = srccolors[j][i][3];
			}
		}
	}

	if((alphabase[0] > alphabase[1]) && !(alphaabsmin && alphaabsmax))
	{
		// one color, either max or min
		// shortcut here since it is a very common case (and also avoids later problems)
		// || (alphabase[0] == alphabase[1] && !alphaabsmin && !alphaabsmax)
		// could also thest for alpha0 == alpha1 (and not min/max), but probably not common, so don't bother

		alphabase[0] = srccolors[0][0][3];
		*blkaddr++ = alphabase[0];
		*blkaddr++;
		*blkaddr++ = 0;
		*blkaddr++ = 0;
		*blkaddr++ = 0;
		*blkaddr++ = 0;
		*blkaddr++ = 0;
		*blkaddr++ = 0;
		return;
	}

	// find best encoding for alpha0 > alpha1
	// it's possible this encoding is better even if both alphaabsmin and alphaabsmax are true
	alphablockerror1 = 0x0;
	alphablockerror2 = 0xffffffff;
	alphablockerror3 = 0xffffffff;

	if( alphaabsmin ) alphause[0] = 0;
	else alphause[0] = alphabase[0];

	if( alphaabsmax ) alphause[1] = 255;
	else alphause[1] = alphabase[1];

	// calculate the 7 cut values, just the middle between 2 of the computed alpha values
	for( aindex = 0; aindex < 7; aindex++ )
	{
		// don't forget here is always rounded down
		acutValues[aindex] = (alphause[0] * (2*aindex + 1) + alphause[1] * (14 - (2*aindex + 1))) / 14;
	}

	for( j = 0; j < numypixels; j++ )
	{
		for( i = 0; i < numxpixels; i++ )
		{
			// maybe it's overkill to have the most complicated calculation just for the error
			// calculation which we only need to figure out if encoding1 or encoding2 is better...
			if( srccolors[j][i][3] > acutValues[0] )
			{
				alphaenc1[4*j + i] = 0;
				alphadist = srccolors[j][i][3] - alphause[1];
			}
			else if( srccolors[j][i][3] > acutValues[1] )
			{
				alphaenc1[4*j + i] = 2;
				alphadist = srccolors[j][i][3] - (alphause[1] * 6 + alphause[0] * 1) / 7;
			}
			else if( srccolors[j][i][3] > acutValues[2] )
			{
				alphaenc1[4*j + i] = 3;
				alphadist = srccolors[j][i][3] - (alphause[1] * 5 + alphause[0] * 2) / 7;
			}
			else if( srccolors[j][i][3] > acutValues[3] )
			{
				alphaenc1[4*j + i] = 4;
				alphadist = srccolors[j][i][3] - (alphause[1] * 4 + alphause[0] * 3) / 7;
			}
			else if( srccolors[j][i][3] > acutValues[4] )
			{
				alphaenc1[4*j + i] = 5;
				alphadist = srccolors[j][i][3] - (alphause[1] * 3 + alphause[0] * 4) / 7;
			}
			else if( srccolors[j][i][3] > acutValues[5] )
			{
				alphaenc1[4*j + i] = 6;
				alphadist = srccolors[j][i][3] - (alphause[1] * 2 + alphause[0] * 5) / 7;
			}
			else if( srccolors[j][i][3] > acutValues[6] )
			{
				alphaenc1[4*j + i] = 7;
				alphadist = srccolors[j][i][3] - (alphause[1] * 1 + alphause[0] * 6) / 7;
			}
			else
			{
				alphaenc1[4*j + i] = 1;
				alphadist = srccolors[j][i][3] - alphause[0];
			}
			alphablockerror1 += alphadist * alphadist;
		}
	}

	// it's not very likely this encoding is better if both alphaabsmin and alphaabsmax
	// are false but try it anyway
	if( alphablockerror1 >= 32 )
	{
		// don't bother if encoding is already very good, this condition should also imply
		// we have valid alphabase colors which we absolutely need (alphabase[0] <= alphabase[1])
		alphablockerror2 = 0;

		for( aindex = 0; aindex < 5; aindex++ )
		{
			// don't forget here is always rounded down
			acutValues[aindex] = (alphabase[0] * (10 - (2*aindex + 1)) + alphabase[1] * (2*aindex + 1)) / 10;
		}

		for( j = 0; j < numypixels; j++ )
		{
			for( i = 0; i < numxpixels; i++ )
			{
				// maybe it's overkill to have the most complicated calculation just for the error
				// calculation which we only need to figure out if encoding1 or encoding2 is better...
				if( srccolors[j][i][3] == 0 )
				{
					alphaenc2[4*j + i] = 6;
					alphadist = 0;
				}
				else if( srccolors[j][i][3] == 255 )
				{
					alphaenc2[4*j + i] = 7;
					alphadist = 0;
				}
				else if( srccolors[j][i][3] <= acutValues[0] )
				{
					alphaenc2[4*j + i] = 0;
					alphadist = srccolors[j][i][3] - alphabase[0];
				}
				else if( srccolors[j][i][3] <= acutValues[1] )
				{
					alphaenc2[4*j + i] = 2;
					alphadist = srccolors[j][i][3] - (alphabase[0] * 4 + alphabase[1] * 1) / 5;
				}
				else if( srccolors[j][i][3] <= acutValues[2] )
				{
					alphaenc2[4*j + i] = 3;
					alphadist = srccolors[j][i][3] - (alphabase[0] * 3 + alphabase[1] * 2) / 5;
				}
				else if( srccolors[j][i][3] <= acutValues[3] )
				{
					alphaenc2[4*j + i] = 4;
					alphadist = srccolors[j][i][3] - (alphabase[0] * 2 + alphabase[1] * 3) / 5;
				}
				else if( srccolors[j][i][3] <= acutValues[4] )
				{
					alphaenc2[4*j + i] = 5;
					alphadist = srccolors[j][i][3] - (alphabase[0] * 1 + alphabase[1] * 4) / 5;
				}
				else
				{
					alphaenc2[4*j + i] = 1;
					alphadist = srccolors[j][i][3] - alphabase[1];
				}
				alphablockerror2 += alphadist * alphadist;
			}
		}

		// skip this if the error is already very small
		// this encoding is MUCH better on average than #2 though, but expensive!
		if((alphablockerror2 > 96) && (alphablockerror1 > 96))
		{
			short	blockerrlin1 = 0;
			short	blockerrlin2 = 0;
			byte	nralphainrangelow = 0;
			byte	nralphainrangehigh = 0;

			alphatest[0] = 0xff;
			alphatest[1] = 0x0;
			// if we have large range it's likely there are values close to 0/255, try to map them to 0/255
			for( j = 0; j < numypixels; j++ )
			{
				for( i = 0; i < numxpixels; i++ )
				{
					if((srccolors[j][i][3] > alphatest[1]) && (srccolors[j][i][3] < (255 -(alphabase[1] - alphabase[0]) / 28)))
						alphatest[1] = srccolors[j][i][3];
					if((srccolors[j][i][3] < alphatest[0]) && (srccolors[j][i][3] > (alphabase[1] - alphabase[0]) / 28))
						alphatest[0] = srccolors[j][i][3];
				}
			}
			// shouldn't happen too often, don't really care about those degenerated cases
			if( alphatest[1] <= alphatest[0] )
			{
				alphatest[0] = 1;
				alphatest[1] = 254;
			}

			for( aindex = 0; aindex < 5; aindex++ )
			{
				// don't forget here is always rounded down
				acutValues[aindex] = (alphatest[0] * (10 - (2*aindex + 1)) + alphatest[1] * (2*aindex + 1)) / 10;
			}

			// find the "average" difference between the alpha values and the next encoded value.
			// this is then used to calculate new base values.
			// should there be some weighting, i.e. those values closer to alphatest[x] have more weight,
			// since they will see more improvement, and also because the values in the middle are somewhat
			// likely to get no improvement at all (because the base values might move in different directions)?
			// OTOH it would mean the values in the middle are even less likely to get an improvement

			for( j = 0; j < numypixels; j++ )
			{
				for( i = 0; i < numxpixels; i++ )
				{
					if( srccolors[j][i][3] <= alphatest[0] / 2 )
					{
						// this does nothing
					}
					else if( srccolors[j][i][3] > ((255 + alphatest[1]) / 2))
					{
						// this does nothing
					}
					else if( srccolors[j][i][3] <= acutValues[0] )
					{
						blockerrlin1 += (srccolors[j][i][3] - alphatest[0]);
						nralphainrangelow += 1;
					}
					else if( srccolors[j][i][3] <= acutValues[1] )
					{
						blockerrlin1 += (srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5);
						blockerrlin2 += (srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5);
						nralphainrangelow += 1;
						nralphainrangehigh += 1;
					}
					else if( srccolors[j][i][3] <= acutValues[2] )
					{
						blockerrlin1 += (srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5);
						blockerrlin2 += (srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5);
						nralphainrangelow += 1;
						nralphainrangehigh += 1;
					}
					else if( srccolors[j][i][3] <= acutValues[3] )
					{
						blockerrlin1 += (srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5);
						blockerrlin2 += (srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5);
						nralphainrangelow += 1;
						nralphainrangehigh += 1;
					}
					else if( srccolors[j][i][3] <= acutValues[4] )
					{
						blockerrlin1 += (srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5);
						blockerrlin2 += (srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5);
						nralphainrangelow += 1;
						nralphainrangehigh += 1;
					}
					else
					{
						blockerrlin2 += (srccolors[j][i][3] - alphatest[1]);
						nralphainrangehigh += 1;
					}
				}
			}

			// shouldn't happen often, needed to avoid div by zero
			if( nralphainrangelow == 0 ) nralphainrangelow = 1;
			if( nralphainrangehigh == 0 ) nralphainrangehigh = 1;
			alphatest[0] = alphatest[0] + (blockerrlin1 / nralphainrangelow);

			// again shouldn't really happen often...
			if( alphatest[0] < 0 )
			{
				alphatest[0] = 0;
			}
			alphatest[1] = alphatest[1] + (blockerrlin2 / nralphainrangehigh);
			if( alphatest[1] > 255 )
			{
				alphatest[1] = 255;
			}
			alphablockerror3 = 0;

			for( aindex = 0; aindex < 5; aindex++ )
			{
				// don't forget here is always rounded down
				acutValues[aindex] = (alphatest[0] * (10 - (2*aindex + 1)) + alphatest[1] * (2*aindex + 1)) / 10;
			}

			for( j = 0; j < numypixels; j++ )
			{
				for( i = 0; i < numxpixels; i++ )
				{
					// maybe it's overkill to have the most complicated calculation just for the error
                  				// calculation which we only need to figure out if encoding1 or encoding2 is better...
					if( srccolors[j][i][3] <= alphatest[0] / 2 )
					{
						alphaenc3[4*j + i] = 6;
						alphadist = srccolors[j][i][3];
					}
					else if( srccolors[j][i][3] > ((255 + alphatest[1]) / 2))
					{
						alphaenc3[4*j + i] = 7;
						alphadist = 255 - srccolors[j][i][3];
					}
					else if( srccolors[j][i][3] <= acutValues[0] )
					{
						alphaenc3[4*j + i] = 0;
						alphadist = srccolors[j][i][3] - alphatest[0];
					}
					else if( srccolors[j][i][3] <= acutValues[1] )
					{
						alphaenc3[4*j + i] = 2;
						alphadist = srccolors[j][i][3] - (alphatest[0] * 4 + alphatest[1] * 1) / 5;
					}
					else if( srccolors[j][i][3] <= acutValues[2] )
					{
						alphaenc3[4*j + i] = 3;
						alphadist = srccolors[j][i][3] - (alphatest[0] * 3 + alphatest[1] * 2) / 5;
					}
					else if( srccolors[j][i][3] <= acutValues[3] )
					{
						alphaenc3[4*j + i] = 4;
						alphadist = srccolors[j][i][3] - (alphatest[0] * 2 + alphatest[1] * 3) / 5;
					}
					else if( srccolors[j][i][3] <= acutValues[4] )
					{
						alphaenc3[4*j + i] = 5;
						alphadist = srccolors[j][i][3] - (alphatest[0] * 1 + alphatest[1] * 4) / 5;
					}
					else
					{
						alphaenc3[4*j + i] = 1;
						alphadist = srccolors[j][i][3] - alphatest[1];
					}
					alphablockerror3 += alphadist * alphadist;
				}
			}
		}
	}

	// write the alpha values and encoding back.
	if((alphablockerror1 <= alphablockerror2) && (alphablockerror1 <= alphablockerror3))
		Image_StoreAlphaBlock( blkaddr, alphause[1], alphause[0], alphaenc1 );
	else if( alphablockerror2 <= alphablockerror3 )
		Image_StoreAlphaBlock( blkaddr, alphabase[0], alphabase[1], alphaenc2 );
	else Image_StoreAlphaBlock( blkaddr, (byte)alphatest[0], (byte)alphatest[1], alphaenc3 );
}

static void Image_ExtractColors( byte srcpixels[4][4][4], const byte *srcaddr, int srcRowStride, int numxpixels, int numypixels, int comps )
{
	const byte *curaddr;
	byte	i, j, c;

	for( j = 0; j < numypixels; j++ )
	{
		curaddr = srcaddr + j * srcRowStride * comps;
		for( i = 0; i < numxpixels; i++ )
		{
			for( c = 0; c < comps; c++ )
				srcpixels[j][i][c] = *curaddr++ / (CHAN_MAX / 255);
		}
	}
}

/*
====================
Image_DXTReadColors

read colors from dxt image
====================
*/
void Image_DXTReadColors( const byte* data, color32* out )
{
	byte r0, g0, b0, r1, g1, b1;

	b0 = data[0] & 0x1F;
	g0 = ((data[0] & 0xE0) >> 5) | ((data[1] & 0x7) << 3);
	r0 = (data[1] & 0xF8) >> 3;

	b1 = data[2] & 0x1F;
	g1 = ((data[2] & 0xE0) >> 5) | ((data[3] & 0x7) << 3);
	r1 = (data[3] & 0xF8) >> 3;

	out[0].r = r0<<3;
	out[0].g = g0<<2;
	out[0].b = b0<<3;

	out[1].r = r1<<3;
	out[1].g = g1<<2;
	out[1].b = b1<<3;
}

/*
====================
Image_DXTReadColor

read one color from dxt image
====================
*/
void Image_DXTReadColor( word data, color32* out )
{
	byte r, g, b;

	b = data & 0x1f;
	g = (data & 0x7E0) >>5;
	r = (data & 0xF800)>>11;

	out->r = r << 3;
	out->g = g << 2;
	out->b = b << 3;
}

void Image_CorrectPreMult( uint *data, int datasize )
{
	int	i;

	for( i = 0; i < datasize; i += 4 )
	{
		if( data[i+3] != 0 ) // don't divide by 0.
		{	
			data[i+0] = (byte)(((uint)data[i+0]<<8) / data[i+3]);
			data[i+1] = (byte)(((uint)data[i+1]<<8) / data[i+3]);
			data[i+2] = (byte)(((uint)data[i+2]<<8) / data[i+3]);
		}
	}
}

/*
====================
Image_GetBitsFromMask
====================
*/
void Image_GetBitsFromMask( uint Mask, uint *ShiftLeft, uint *ShiftRight )
{
	uint Temp, i;

	if( Mask == 0 )
	{
		*ShiftLeft = *ShiftRight = 0;
		return;
	}

	Temp = Mask;
	for (i = 0; i < 32; i++, Temp >>= 1)
	{
		if (Temp & 1) break;
	}
	*ShiftRight = i;

	// Temp is preserved, so use it again:
	for (i = 0; i < 8; i++, Temp >>= 1)
	{
		if(!(Temp & 1)) break;
	}
	*ShiftLeft = 8 - i;
}

bool Image_DXTWriteHeader( vfile_t *f, rgbdata_t *pix )
{
	uint	dwFourCC = 0, dwFlags1 = 0, dwFlags2 = 0, dwCaps1 = 0, dwCaps2 = 0;
	uint	dwLinearSize, dwBlockSize, dwSize = 124, dwSize2 = sizeof( dds_pixf_t );
	uint	dwRBitMask = 0, dwGBitMask = 0, dwBBitMask = 0, dwABitMask = 0;
	uint	dwWidth, dwHeight, dwDepth, dwMipCount;
	uint	dwIdent = DDSHEADER;
	uint	dwRGBBitCount = 0;

	if( !pix || !pix->buffer ) return false;

	// setup flags
	dwFlags1 |= DDS_LINEARSIZE|DDS_WIDTH|DDS_HEIGHT|DDS_CAPS|DDS_PIXELFORMAT|DDS_PITCH;
	dwFlags2 |= DDS_FOURCC;
	dwMipCount = pix->numMips;
	if( dwMipCount > 1 ) dwFlags1 |= DDS_MIPMAPCOUNT;
	if( pix->numLayers > 1 ) dwFlags1 |= DDS_DEPTH;

	switch( pix->type )
	{
	case PF_DXT1:
		dwFourCC = TYPE_DXT1;
		break;
	case PF_DXT2:
		dwFourCC = TYPE_DXT2;
		break;
	case PF_DXT3:
		dwFourCC = TYPE_DXT3;
		break;
	case PF_DXT4:
		dwFourCC = TYPE_DXT4;
		break;
	case PF_DXT5:
		dwFourCC = TYPE_DXT5;
		break;
	case PF_ABGR_64:
		dwFourCC = TYPE_$;
		break;
	case PF_R_16F:
		dwFourCC = TYPE_o;
		break;
	case PF_GR_32F:
		dwFourCC = TYPE_p;
		break;
	case PF_ABGR_64F:
		dwFourCC = TYPE_q;
		break;
	case PF_R_32F:
		dwFourCC = TYPE_r;
		break;
	case PF_GR_64F:
		dwFourCC = TYPE_s;
		break;
	case PF_ABGR_128F:
		dwFourCC = TYPE_t;
		break;
	default:
		dwFlags2 &= ~DDS_FOURCC;
		break;
	}

	// continue choosing format
	if(!(dwFlags2 & DDS_FOURCC ))
	{
		switch( pix->type )
		{
		case PF_LUMINANCE:
			dwABitMask = 0x00FFFFFF;
			dwFlags2 |= DDS_LUMINANCE;
			dwRGBBitCount = 8; 
			break;
		case PF_LUMINANCE_ALPHA:
			dwRBitMask = 0x00FF0000;
			dwABitMask = 0xFF000000;
			dwFlags2 |= DDS_LUMINANCE;
			dwFlags2 |= DDS_ALPHAPIXELS;
			dwRGBBitCount = 16; 
			break;
		case PF_BGR_24:
			dwRBitMask = 0x000000FF;
			dwGBitMask = 0x0000FF00;
			dwBBitMask = 0x00FF0000;
			dwFlags2 |= DDS_RGB;
			dwRGBBitCount = 24; 
			break;
		case PF_BGRA_32:
			dwRBitMask = 0x000000FF;
			dwGBitMask = 0x0000FF00;
			dwBBitMask = 0x00FF0000;
			dwABitMask = 0xFF000000;
			dwFlags2 |= DDS_RGBA;
			dwRGBBitCount = 32; 
			break;
		case PF_RGBA_32:
			dwRBitMask = 0x00FF0000;
			dwGBitMask = 0x0000FF00;
			dwBBitMask = 0x000000FF;
			dwABitMask = 0xFF000000;
			dwFlags2 |= DDS_RGBA;
			dwRGBBitCount = 32; 
			break;
		default:
			MsgDev( D_ERROR, "Image_DXTWriteHeader: unsupported type %s\n", PFDesc[pix->type].name );	
			return false;
		}
	}

	dwWidth = pix->width;
	dwHeight = pix->height;

	VFS_Write( f, &dwIdent, sizeof(uint));
	VFS_Write( f, &dwSize, sizeof(uint));
	VFS_Write( f, &dwFlags1, sizeof(uint));
	VFS_Write( f, &dwHeight, sizeof(uint));
	VFS_Write( f, &dwWidth, sizeof(uint));

	dwBlockSize = PFDesc[pix->type].block;
	dwLinearSize = Image_DXTGetLinearSize( pix->type, pix->width, pix->height, 1, pix->bitsCount / 8 );
	VFS_Write( f, &dwLinearSize, sizeof( uint ));

	if( pix->numLayers > 1 )
	{
		dwDepth = pix->numLayers;
		VFS_Write( f, &dwDepth, sizeof( uint ));
		dwCaps2 |= DDS_VOLUME;
	}
	else VFS_Write( f, 0, sizeof( uint ));
	VFS_Write( f, &dwMipCount, sizeof( uint ));
	VFS_Write( f, 0, sizeof(uint));
	VFS_Write( f, 0, sizeof(uint) * 10 ); // reserved fields

	VFS_Write( f, &dwSize2, sizeof( uint ));
	VFS_Write( f, &dwFlags2, sizeof( uint ));
	VFS_Write( f, &dwFourCC, sizeof( uint ));
	VFS_Write( f, &dwRGBBitCount, sizeof( uint ));
	VFS_Write( f, &dwRBitMask, sizeof( uint ));
	VFS_Write( f, &dwGBitMask, sizeof( uint ));
	VFS_Write( f, &dwBBitMask, sizeof( uint ));
	VFS_Write( f, &dwABitMask, sizeof( uint ));

	dwCaps1 |= DDS_TEXTURE;
	if( pix->numMips > 1 ) dwCaps1 |= DDS_MIPMAP|DDS_COMPLEX;
	if( pix->flags & IMAGE_CUBEMAP )
	{
		dwCaps1 |= DDS_COMPLEX;
		dwCaps2 |= DDS_CUBEMAP;
		dwCaps2 |= DDS_CUBEMAP_ALL_SIDES;
	}

	VFS_Write( f, &dwCaps1, sizeof( uint ));
	VFS_Write( f, &dwCaps2, sizeof( uint ));
	VFS_Write( f, 0, sizeof( uint ) * 3 ); // other caps and TextureStage

	return true;
}

size_t Image_DXTGetLinearSize( int type, int width, int height, int depth, int rgbcount )
{
	size_t BlockSize = 0;
	int block, bpp;

	// right calcualte blocksize
	block = PFDesc[type].block;
	bpp = PFDesc[type].bpp;

	if( block == 0 ) BlockSize = width * height * bpp;
	else if( block > 0 ) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if( block < 0 && rgbcount > 0 ) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * abs( block );

	return BlockSize;
}

size_t Image_CompressDXT( vfile_t *f, int saveformat, rgbdata_t *pix )
{
	byte	srcpixels[4][4][4];
	int	numxpixels, numypixels;
	int	i, j, width, height;
	const byte *srcaddr;
	byte	*blkaddr;	
	size_t	dst_size;
	int	srccomps;

	if( !pix || !pix->buffer )
		return 0;

	width = pix->width;
	height = pix->height;
	dst_size = Image_DXTGetLinearSize( saveformat, width, height, 1, 0 );
	srccomps = PFDesc[pix->type].bpp;
	blkaddr = image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, dst_size ); // alloc dst image size

	switch( saveformat )
	{
	case PF_DXT1:
		for( j = 0; j < height; j += 4 )
		{
			if( height > j + 3 ) numypixels = 4;
			else numypixels = height - j;
			srcaddr = pix->buffer + j * width * srccomps;
         			for( i = 0; i < width; i += 4 )
         			{
				if( width > i + 3 ) numxpixels = 4;
				else numxpixels = width - i;
				Image_ExtractColors( srcpixels, srcaddr, width, numxpixels, numypixels, srccomps );
				Image_EncodeColorBlock( blkaddr, srcpixels, numxpixels, numypixels, saveformat, pix->flags );
				srcaddr += srccomps * numxpixels;
				blkaddr += 8;
			}
		}
		break;
	case PF_DXT3:
		for( j = 0; j < height; j += 4 )
		{
			if( height > j + 3 ) numypixels = 4;
			else numypixels = height - j;
			srcaddr = pix->buffer + j * width * srccomps;

			for( i = 0; i < width; i += 4 )
			{
				if( width > i + 3 ) numxpixels = 4;
				else numxpixels = width - i;
				Image_ExtractColors( srcpixels, srcaddr, width, numxpixels, numypixels, srccomps );
				*blkaddr++ = (srcpixels[0][0][3] >> 4) | (srcpixels[0][1][3] & 0xf0);
				*blkaddr++ = (srcpixels[0][2][3] >> 4) | (srcpixels[0][3][3] & 0xf0);
				*blkaddr++ = (srcpixels[1][0][3] >> 4) | (srcpixels[1][1][3] & 0xf0);
				*blkaddr++ = (srcpixels[1][2][3] >> 4) | (srcpixels[1][3][3] & 0xf0);
				*blkaddr++ = (srcpixels[2][0][3] >> 4) | (srcpixels[2][1][3] & 0xf0);
				*blkaddr++ = (srcpixels[2][2][3] >> 4) | (srcpixels[2][3][3] & 0xf0);
				*blkaddr++ = (srcpixels[3][0][3] >> 4) | (srcpixels[3][1][3] & 0xf0);
				*blkaddr++ = (srcpixels[3][2][3] >> 4) | (srcpixels[3][3][3] & 0xf0);
				Image_EncodeColorBlock( blkaddr, srcpixels, numxpixels, numypixels, saveformat, pix->flags );
				srcaddr += srccomps * numxpixels;
				blkaddr += 8;
			}
		}
		break;
	case PF_DXT5:
		for( j = 0; j < height; j += 4 )
		{
			if( height > j + 3 ) numypixels = 4;
			else numypixels = height - j;
			srcaddr = pix->buffer + j * width * srccomps;

			for( i = 0; i < width; i += 4 )
			{
				if( width > i + 3 ) numxpixels = 4;
				else numxpixels = width - i;

				Image_ExtractColors( srcpixels, srcaddr, width, numxpixels, numypixels, srccomps );
				Image_EncodeDXT5alpha( blkaddr, srcpixels, numxpixels, numypixels );
				Image_EncodeColorBlock( blkaddr + 8, srcpixels, numxpixels, numypixels, saveformat, pix->flags );
				srcaddr += srccomps * numxpixels;
				blkaddr += 16;
			}
		}
		break;
	default:
      		MsgDev( D_ERROR, "Image_CompressDXT: bad destination format %s\n", PFDesc[saveformat].name );
		return 0;
	}
	dst_size = VFS_Write( f, image.tempbuffer, dst_size );

	return dst_size;
}

void Image_CompressDDS( vfile_t *f, byte *buffer )
{
#if 0
	int i, size = 0;
	int w = image.curwidth;
	int h = image.curheight;
	int d = image.curdepth;

	for( i = 0; i < image.cur_mips; i++, buffer += size )
	{
		Image_SetPixelFormat( w, h, d );
		size = image.SizeOfFile;

		if(!image.decompress( target, i, image.type, w, h, size, buffer ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}
#endif
}

bool Image_DXTWriteImage( vfile_t *f, rgbdata_t *pix )
{
	// just write input buffer as valid
	switch( pix->type )
	{
	case PF_BGR_24:
	case PF_BGRA_32:
	case PF_RGBA_32:
	case PF_LUMINANCE:
	case PF_LUMINANCE_ALPHA:
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
	case PF_ABGR_64F:
		VFS_Write( f, pix->buffer, pix->size );
		return true;
	}
	return false;
}

void Image_DXTGetPixelFormat( dds_t *hdr )
{
	uint bits = hdr->dsPixelFormat.dwRGBBitCount;

	// All volume textures I've seem so far didn't have the DDS_COMPLEX flag set,
	// even though this is normally required. But because noone does set it,
	// also read images without it (TODO: check file size for 3d texture?)
	if (!(hdr->dsCaps.dwCaps2 & DDS_VOLUME)) hdr->dwDepth = 1;

	if( hdr->dsPixelFormat.dwFlags & DDS_ALPHA )
		image.flags |= IMAGE_HAVE_ALPHA;

	if( hdr->dsPixelFormat.dwFlags & DDS_FOURCC )
	{
		switch( hdr->dsPixelFormat.dwFourCC )
		{
		case TYPE_DXT1: image.type = PF_DXT1; break;
		case TYPE_DXT2: image.type = PF_DXT2; break;
		case TYPE_DXT3: image.type = PF_DXT3; break;
		case TYPE_DXT4: image.type = PF_DXT4; break;
		case TYPE_DXT5: image.type = PF_DXT5; break;
		case TYPE_ATI1: image.type = PF_ATI1N; break;
		case TYPE_ATI2: image.type = PF_ATI2N; break;
		case TYPE_RXGB: image.type = PF_RXGB; break;
		case TYPE_$: image.type = PF_ABGR_64; break;
		case TYPE_o: image.type = PF_R_16F; break;
		case TYPE_p: image.type = PF_GR_32F; break;
		case TYPE_q: image.type = PF_ABGR_64F; break;
		case TYPE_r: image.type = PF_R_32F;
		case TYPE_s: image.type = PF_GR_64F;
		case TYPE_t: image.type = PF_ABGR_128F; break;
		default: image.type = PF_UNKNOWN; break;
		}
	}
	else
	{
		// this dds texture isn't compressed so write out ARGB or luminance format
		if( hdr->dsPixelFormat.dwFlags & DDS_LUMINANCE )
		{
			if( hdr->dsPixelFormat.dwFlags & DDS_ALPHAPIXELS )
				image.type = PF_LUMINANCE_ALPHA;
			else if( hdr->dsPixelFormat.dwRGBBitCount == 16 && hdr->dsPixelFormat.dwRBitMask == 0xFFFF ) 
				image.type = PF_LUMINANCE_16;
			else image.type = PF_LUMINANCE;
		}
		else 
		{
			if( bits == 32) image.type = PF_ABGR_64;
			else image.type = PF_ARGB_32;
		}
	}

	// setup additional flags
	if( hdr->dsCaps.dwCaps1 & DDS_COMPLEX && hdr->dsCaps.dwCaps2 & DDS_CUBEMAP )
		image.flags |= IMAGE_CUBEMAP;

	if( hdr->dsPixelFormat.dwFlags & DDS_ALPHAPIXELS )
		image.flags |= IMAGE_HAVE_ALPHA;

	if( hdr->dwFlags & DDS_MIPMAPCOUNT )
		image.num_mips = hdr->dwMipMapCount;
	else image.num_mips = 1;

	if( image.type == PF_ARGB_32 || image.type == PF_LUMINANCE || image.type == PF_LUMINANCE_16 || image.type == PF_LUMINANCE_ALPHA )
	{
		// store RGBA mask into palette space
		byte *tmp = image.palette = Mem_Alloc( Sys.imagepool, sizeof(uint) * 4 );
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwRBitMask, sizeof( uint )); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwGBitMask, sizeof( uint )); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwBBitMask, sizeof( uint )); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwABitMask, sizeof( uint )); tmp += 4;
	}
}

void Image_DXTAdjustVolume( dds_t *hdr )
{
	uint bits;
	
	if( hdr->dwDepth <= 1 ) return;
	bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
	hdr->dwFlags |= DDS_LINEARSIZE;
	hdr->dwLinearSize = Image_DXTGetLinearSize( image.type, hdr->dwWidth, hdr->dwHeight, hdr->dwDepth, bits );
}

uint Image_DXTCalcMipmapSize( dds_t *hdr ) 
{
	uint buffsize = 0;
	int w = hdr->dwWidth;
	int h = hdr->dwHeight;
	int d = hdr->dwDepth;
	int i, mipsize = 0;
	int bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
		
	// now correct buffer size
	for( i = 0; i < image.num_mips; i++, buffsize += mipsize )
	{
		mipsize = Image_DXTGetLinearSize( image.type, w, h, d, bits );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}
	return buffsize;
}

uint Image_DXTCalcSize( const char *name, dds_t *hdr, size_t filesize ) 
{
	size_t buffsize = 0;
	int w = image.width;
	int h = image.height;
	int d = image.num_layers;
	int bits = hdr->dsPixelFormat.dwRGBBitCount / 8;

	if( hdr->dsCaps.dwCaps2 & DDS_CUBEMAP ) 
	{
		// cubemap w*h always match for all sides
		buffsize = Image_DXTCalcMipmapSize( hdr ) * 6;
	}
	else if( hdr->dwFlags & DDS_MIPMAPCOUNT )
	{
		// if mipcount > 1
		buffsize = Image_DXTCalcMipmapSize( hdr );
	}
	else if( hdr->dwFlags & (DDS_LINEARSIZE|DDS_PITCH))
	{
		// just in case (no need, really)
		buffsize = hdr->dwLinearSize;
	}
	else 
	{
		// pretty solution for microsoft bug
		buffsize = Image_DXTCalcMipmapSize( hdr );
	}

	if( filesize != buffsize ) // main check
	{
		MsgDev( D_WARN, "LoadDDS: (%s) probably corrupted(%i should be %i)\n", name, buffsize, filesize );
		return false;
	}
	return buffsize;
}

void Image_AddRGBAToPack( uint target, int level, uint imageSize, const void* data )
{
	// NOTE: just update bufer without checking for type
	image.rgba = Mem_Realloc( Sys.imagepool, image.rgba, image.ptr + imageSize );
	Mem_Copy( image.rgba + image.ptr, data, imageSize ); // add mipmap or cubemapside

	image.size += imageSize;	// update image size
	image.ptr += imageSize;
	if( level ) image.num_mips++;
}

/*
=============
Image_SetPixelFormat

update pixel format for miplevel or layer
=============
*/
void Image_SetPixelFormat( int width, int height, int depth )
{
	image.bpp = PFDesc[image.type].bpp;
	image.bpc = PFDesc[image.type].bpc;

	image.curdepth = depth;
	image.curwidth = width;
	image.curheight = height;

	image.bps = image.curwidth * image.bpp * image.bpc;
	image.SizeOfPlane = image.bps * image.curheight;
	image.SizeOfData = image.SizeOfPlane * image.curdepth;

	// NOTE: size of current miplevel or cubemap side, not total (filesize - sizeof(header))
	image.SizeOfFile = Image_DXTGetLinearSize( image.type, width, height, depth, image.bits_count / 8 );
}

bool Image_DecompressFloat( uint target, int level, int intformat, uint width, uint height, uint imageSize, const void* data )
{
	uint	floatformat = 0;
	uint	i, size = 0;
	uint	pixformat = PFDesc[intformat].format;
	word	*src = (word *)data;
	uint	*dest;

	if( !src ) return false;

	switch( pixformat )
	{
	case PF_R_16F:
	case PF_GR_32F:
	case PF_ABGR_64F:
		size = width * height * image.curdepth * image.bpp;
		image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
		dest = (uint *)image.tempbuffer;
		for( i = 0; i < size; i++, dest++, src++ )
			*dest = Image_ShortToFloat( *src );
		break;
	case PF_R_32F:
	case PF_GR_64F:
	case PF_ABGR_128F:
		size = image.SizeOfData;
		image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
		dest = (uint *)image.tempbuffer;
		Mem_Copy( dest, data, image.SizeOfData );
		break;
	default: return false;
	}
	Image_AddRGBAToPack( target, level, size, dest );
	return true;
}

bool Image_DecompressATI( uint target, int level, int intformat, uint width, uint height, uint imageSize, const void* data )
{
	int	x, y, z, w, h, i, j, k, t1, t2, size;
	byte	Colours[8], XColours[8], YColours[8];
	uint	bitmask, bitmask2, CurrOffset, Offset = 0;
	byte	*fin, *fin2, *fout;

	if( !data ) return false;
	fin = (byte *)data;

	w = width;
	h = height;
	size = width * height * image.curdepth * 4;
	image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
	fout = image.tempbuffer;

	switch( PFDesc[intformat].format )
	{
	case PF_ATI1N:
		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					// read palette
					t1 = Colours[0] = fin[0];
					t2 = Colours[1] = fin[1];
					fin += 2;

					if( t1 > t2 )
					{
						for( i = 2; i < 8; ++i )
							Colours[i] = t1 + ((t2 - t1)*(i - 1)) / 7;
					}
					else
					{
						for( i = 2; i < 6; ++i )
							Colours[i] = t1 + ((t2 - t1)*(i - 1)) / 5;
						Colours[6] = 0;
						Colours[7] = 0xFF;
					}
					// decompress pixel data
					CurrOffset = Offset;
					for( k = 0; k < 4; k += 2 )
					{
						// first three bytes
						bitmask = ((uint)(fin[0]) << 0) | ((uint)(fin[1]) << 8) | ((uint)(fin[2]) << 16);

						for( j = 0; j < 2; j++ )
						{
							// only put pixels out < height
							if((y + k + j) < h )
							{
								for( i = 0; i < 4; i++ )
								{
									// only put pixels out < width
									if((x + i) < w )
									{
										t1 = CurrOffset + (x + i);
										fout[t1] = Colours[bitmask & 0x07];
									}
									bitmask >>= 3;
								}
								CurrOffset += image.bps;
							}
						}
						fin += 3;
					}
				}
				Offset += image.bps * 4;
			}
		}
		break;
	case PF_ATI2N:
		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					fin2 = fin + 8;

					// read Y palette
					t1 = YColours[0] = fin[0];
					t2 = YColours[1] = fin[1];
					fin += 2;

					if( t1 > t2 )
					{
						for( i = 2; i < 8; ++i )
							YColours[i] = t1 + ((t2 - t1) * (i - 1)) / 7;
					}
					else
					{
						for( i = 2; i < 6; ++i )
							YColours[i] = t1 + ((t2 - t1) * (i - 1)) / 5;
						YColours[6] = 0;
						YColours[7] = 0xFF;
					}

					// read X palette
					t1 = XColours[0] = fin2[0];
					t2 = XColours[1] = fin2[1];
					fin2 += 2;

					if( t1 > t2 )
					{
						for( i = 2; i < 8; ++i )
							XColours[i] = t1 + ((t2 - t1) * (i - 1)) / 7;
					}
					else
					{
						for( i = 2; i < 6; ++i )
							XColours[i] = t1 + ((t2 - t1) * (i - 1)) / 5;
						XColours[6] = 0;
						XColours[7] = 0xFF;
					}
					// decompress pixel data
					CurrOffset = Offset;

					for( k = 0; k < 4; k += 2 )
					{
						// first three bytes
						bitmask = ((uint)(fin[0]) << 0) | ((uint)(fin[1]) << 8) | ((uint)(fin[2]) << 16);
						bitmask2 = ((uint)(fin2[0]) << 0) | ((uint)(fin2[1]) << 8) | ((uint)(fin2[2]) << 16);

						for( j = 0; j < 2; j++ )
						{
							// only put pixels out < height
							if((y + k + j) < h)
							{
								for( i = 0; i < 4; i++ )
								{
									// only put pixels out < width
									if((x + i) < w )
									{
										int t, tx, ty;

										t1 = CurrOffset + (x + i)*3;
										fout[t1+1] = ty = YColours[bitmask & 0x07];
										fout[t1+0] = tx = XColours[bitmask2 & 0x07];

										// calculate b (z) component ((r/255)^2 + (g/255)^2 + (b/255)^2 = 1
										t = 127 * 128 - (tx - 127) * (tx - 128) - (ty - 127) * (ty - 128);

										if( t > 0 ) fout[t1+2] = (byte)(sqrt(t) + 128); // FIXME: use FastSqrt
										else fout[t1+2] = 0x7F;
									}
									bitmask >>= 3;
									bitmask2 >>= 3;
								}
								CurrOffset += image.bps;
							}
						}
						fin += 3;
						fin2 += 3;
					}
					// skip bytes that were read via Temp2
					fin += 8;
				}
				Offset += image.bps * 4;
			}
		}
		break;
	default: return false;
	}

	Image_AddRGBAToPack( target, level, size, fout );
	return true;
}

bool Image_DecompressDXT( uint target, int level, int intformat, uint width, uint height, uint imageSize, const void* data )
{
	color32	colours[4], *col;
	color16	*color_0, *color_1;
	word	sAlpha, sColor0, sColor1;
	uint	bits, bitmask, Offset, size;
	byte	alphas[8], *alpha, *alphamask; 
	int	w, h, x, y, z, i, j, k, Select; 
	bool	has_alpha = false;
	byte	*fin, *fout;

	if( !data ) return false;
	fin = (byte *)data;

	w = width;
	h = height;

	size = width * height * image.curdepth * 4;
	image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
	fout = image.tempbuffer;
	
	switch( PFDesc[intformat].format )
	{
	case PF_DXT1:
		colours[0].a = 0xFF;
		colours[1].a = 0xFF;
		colours[2].a = 0xFF;

		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					sColor0 = *((word*)fin);
					sColor0 = LittleShort(sColor0);
					sColor1 = *((word*)(fin + 2));
					sColor1 = LittleShort(sColor1);

					Image_DXTReadColor(sColor0, colours);
					Image_DXTReadColor(sColor1, colours + 1);

					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong( bitmask );
					fin += 8;

					if (sColor0 > sColor1)
					{
						// four-color block: derive the other two colors.
						// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
						// These 2-bit codes correspond to the 2-bit fields 
						// stored in the 64-bit block.
						colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
						colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
						colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
						colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
						colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
						colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
						colours[3].a = 0xFF;
					}
					else
					{ 
						// three-color block: derive the other color.
						// 00 = color_0,  01 = color_1,  10 = color_2,
						// 11 = transparent.
						// These 2-bit codes correspond to the 2-bit fields 
						// stored in the 64-bit block. 
						colours[2].b = (colours[0].b + colours[1].b) / 2;
						colours[2].g = (colours[0].g + colours[1].g) / 2;
						colours[2].r = (colours[0].r + colours[1].r) / 2;
						colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
						colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
						colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
						colours[3].a = 0x00;
					}
					for( j = 0, k = 0; j < 4; j++ )
					{
						for( i = 0; i < 4; i++, k++ )
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];

							if (((x + i) < w) && ((y + j) < h))
							{
								uint ofs = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
								fout[ofs+0] = col->r;
								fout[ofs+1] = col->g;
								fout[ofs+2] = col->b;
								fout[ofs+3] = col->a;
								if( col->a == 0 ) has_alpha = true;
							}
						}
					}
				}
			}
		}
		break;
	case PF_DXT2:
		image.flags |= IMAGE_PREMULT;
		// intentional fallthrough
	case PF_DXT3:
		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					alpha = fin;
					fin += 8;
					Image_DXTReadColors( fin, colours );
					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong( bitmask );
					fin += 8;

					// four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// These 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;

					k = 0;
					for( j = 0; j < 4; j++ )
					{
						for( i = 0; i < 4; i++, k++ )
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];
							if(((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
								fout[Offset + 0] = col->r;
								fout[Offset + 1] = col->g;
								fout[Offset + 2] = col->b;
							}
						}
					}
					for( j = 0; j < 4; j++ )
					{
						sAlpha = alpha[2*j] + 256*alpha[2*j+1];
						for( i = 0; i < 4; i++ )
						{
							if(((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp + 3;
								fout[Offset] = sAlpha & 0x0F;
								fout[Offset] = fout[Offset] | (fout[Offset]<<4);
								if(sAlpha == 0) has_alpha = true;
							}
							sAlpha >>= 4;
						}
					}
				}
			}
		}
		break;
	case PF_DXT4:
		image.flags |= IMAGE_PREMULT;
		// intentional fallthrough
	case PF_DXT5:
		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					if( y >= h || x >= w ) break;
					alphas[0] = fin[0];
					alphas[1] = fin[1];
					alphamask = fin + 2;
					fin += 8;

					Image_DXTReadColors(fin, colours);
					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong(bitmask);
					fin += 8;

					// four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// these 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;

					k = 0;
					for( j = 0; j < 4; j++ )
					{
						for( i = 0; i < 4; i++, k++ )
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];
							// only put pixels out < width or height
							if(((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
								fout[Offset+0] = col->r;
								fout[Offset+1] = col->g;
								fout[Offset+2] = col->b;
							}
						}
					}
					// 8-alpha or 6-alpha block?    
					if( alphas[0] > alphas[1] )
					{    
						// 8-alpha block:  derive the other six alphas.    
						// bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
						alphas[2] = (6 * alphas[0] + 1 * alphas[1] + 3) / 7; // bit code 010
						alphas[3] = (5 * alphas[0] + 2 * alphas[1] + 3) / 7; // bit code 011
						alphas[4] = (4 * alphas[0] + 3 * alphas[1] + 3) / 7; // bit code 100
						alphas[5] = (3 * alphas[0] + 4 * alphas[1] + 3) / 7; // bit code 101
						alphas[6] = (2 * alphas[0] + 5 * alphas[1] + 3) / 7; // bit code 110
						alphas[7] = (1 * alphas[0] + 6 * alphas[1] + 3) / 7; // bit code 111
					}
					else
					{
						// 6-alpha block.
						// bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
						alphas[2] = (4 * alphas[0] + 1 * alphas[1] + 2) / 5; // bit code 010
						alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5; // bit code 011
						alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5; // bit code 100
						alphas[5] = (1 * alphas[0] + 4 * alphas[1] + 2) / 5; // bit code 101
						alphas[6] = 0x00;				// bit code 110
						alphas[7] = 0xFF;				// bit code 111
					}
					// NOTE: Have to separate the next two loops,
					// it operates on a 6-byte system.

					// first three bytes
					bits = (alphamask[0]) | (alphamask[1] << 8) | (alphamask[2] << 16);
					for( j = 0; j < 2; j++ )
					{
						for( i = 0; i < 4; i++ )
						{
							// only put pixels out < width or height
							if(((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp + 3;
								fout[Offset] = alphas[bits & 0x07];
							}
							bits >>= 3;
						}
					}
					// last three bytes
					bits = (alphamask[3]) | (alphamask[4] << 8) | (alphamask[5] << 16);
					for( j = 2; j < 4; j++ )
					{
						for( i = 0; i < 4; i++ )
						{
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp + 3;
								fout[Offset] = alphas[bits & 0x07];
								if( bits & 0x07 ) has_alpha = true; 
							}
							bits >>= 3;
						}
					}
				}
			}
		}
		break;
	case PF_RXGB:
		for( z = 0; z < image.curdepth; z++ )
		{
			for( y = 0; y < h; y += 4 )
			{
				for( x = 0; x < w; x += 4 )
				{
					if( y >= h || x >= w ) break;
					alphas[0] = fin[0];
					alphas[1] = fin[1];
					alphamask = fin + 2;
					fin += 8;

					color_0 = ((color16*)fin);
					color_1 = ((color16*)(fin+2));
					bitmask = ((uint*)fin)[1];
					fin += 8;

					colours[0].r = color_0->r << 3;
					colours[0].g = color_0->g << 2;
					colours[0].b = color_0->b << 3;
					colours[0].a = 0xFF;
					colours[1].r = color_1->r << 3;
					colours[1].g = color_1->g << 2;
					colours[1].b = color_1->b << 3;
					colours[1].a = 0xFF;

					// four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// these 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[2].a = 0xFF;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
					colours[3].a = 0xFF;

				k = 0;
				for( j = 0; j < 4; j++ )
				{
					for( i = 0; i < 4; i++, k++ )
					{
						Select = (bitmask & (0x03 << k*2)) >> k*2;
						col = &colours[Select];

						// only put pixels out < width or height
						if(((x + i) < w) && ((y + j) < h))
						{
							Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
							fout[Offset + 0] = col->r;
							fout[Offset + 1] = col->g;
							fout[Offset + 2] = col->b;
						}
					}
				}

				// 8-alpha or 6-alpha block?    
				if (alphas[0] > alphas[1])
				{    
					// 8-alpha block:  derive the other six alphas.    
					// bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
					alphas[2] = (6 * alphas[0] + 1 * alphas[1] + 3) / 7; // bit code 010
					alphas[3] = (5 * alphas[0] + 2 * alphas[1] + 3) / 7; // bit code 011
					alphas[4] = (4 * alphas[0] + 3 * alphas[1] + 3) / 7; // bit code 100
					alphas[5] = (3 * alphas[0] + 4 * alphas[1] + 3) / 7; // bit code 101
					alphas[6] = (2 * alphas[0] + 5 * alphas[1] + 3) / 7; // bit code 110
					alphas[7] = (1 * alphas[0] + 6 * alphas[1] + 3) / 7; // bit code 111
				}
				else
				{
					// 6-alpha block.
					// bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
					alphas[2] = (4 * alphas[0] + 1 * alphas[1] + 2) / 5; // bit code 010
					alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5; // bit code 011
					alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5; // bit code 100
					alphas[5] = (1 * alphas[0] + 4 * alphas[1] + 2) / 5; // bit code 101
					alphas[6] = 0x00;				// bit code 110
					alphas[7] = 0xFF;				// bit code 111
				}

				// NOTE: Have to separate the next two loops,
				// it operates on a 6-byte system.
				// first three bytes

				bits = *((int*)alphamask);
				for( j = 0; j < 2; j++ )
				{
					for( i = 0; i < 4; i++ )
					{
						// only put pixels out < width or height
						if(((x + i) < w) && ((y + j) < h))
						{
							Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
							fout[Offset] = alphas[bits & 0x07];
						}
						bits >>= 3;
					}
				}

				// last three bytes
				bits = *((int*)&alphamask[3]);

				for( j = 2; j < 4; j++ )
				{
					for( i = 0; i < 4; i++ )
					{
						// only put pixels out < width or height
						if(((x + i) < w) && ((y + j) < h))
						{
							Offset = z * image.SizeOfPlane + (y + j) * image.bps + (x + i) * image.bpp;
							fout[Offset] = alphas[bits & 0x07];
						}
						bits >>= 3;
					}
				}
			}
		}
	}
	default: return false;
	}

	// make some post operations
	if( has_alpha ) image.flags |= IMAGE_HAVE_ALPHA;
	if( image.flags & IMAGE_PREMULT )
	{
		Image_CorrectPreMult((uint *)fout, size );
		image.flags &= ~IMAGE_PREMULT;
	}

	Image_AddRGBAToPack( target, level, size, fout );
	return true;
}

bool Image_DecompressARGB( uint target, int level, int intformat, uint width, uint height, uint imageSize, const void* data )
{
	uint	ReadI = 0, TempBpp;
	uint	RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
	uint	r_bitmask, g_bitmask, b_bitmask, a_bitmask;
	int	i, w, h, size;
	byte	*fin, *fout;

	if( !data ) return false;

	w = width;
	h = height;
	fin = (byte *)data;
	size = width * height * image.curdepth * 4;
	image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
	fout = image.tempbuffer;

	if( PFDesc[intformat].format == PF_LUMINANCE_16 )
	{
		Mem_Copy( fout, data, image.SizeOfData);
	}
	else if( image.palette )
	{
		byte *pal = image.palette; //copy ptr
		r_bitmask	= BuffLittleLong( pal ); pal += 4;
		g_bitmask = BuffLittleLong( pal ); pal += 4;
		b_bitmask = BuffLittleLong( pal ); pal += 4;
		a_bitmask = BuffLittleLong( pal ); pal += 4;
	}
	else return false; // rgba mask unset

	Image_GetBitsFromMask( r_bitmask, &RedL, &RedR );
	Image_GetBitsFromMask( g_bitmask, &GreenL, &GreenR );
	Image_GetBitsFromMask( b_bitmask, &BlueL, &BlueR );
	Image_GetBitsFromMask( a_bitmask, &AlphaL, &AlphaR );
          
	TempBpp = image.bits_count / 8;

	for( i = 0; i < image.SizeOfData; i += image.bpp )
	{
		// TODO: This is SLOOOW...
		// but the old version crashed in release build under
		// winxp (and xp is right to stop this code - I always
		// wondered that it worked the old way at all)
		if( image.SizeOfData - i < 4 )
		{
			// less than 4 byte to write?
			if( TempBpp == 1 ) ReadI = *((byte*) fin );
			else if( TempBpp == 2 ) ReadI = BuffLittleShort( fin );
			else if( TempBpp == 3 ) ReadI = BuffLittleLong( fin );
		}
		else ReadI = BuffLittleLong( fin );
		fin += TempBpp;
		fout[i] = ((ReadI & r_bitmask)>> RedR) << RedL;

		if( image.bpp >= 3 )
		{
			fout[i+1] = ((ReadI & g_bitmask) >> GreenR) << GreenL;
			fout[i+2] = ((ReadI & b_bitmask) >> BlueR) << BlueL;
			if( image.bpp == 4 )
			{
				fout[i+3] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
				if( AlphaL >= 7 ) fout[i+3] = fout[i+3] ? 0xFF : 0x00;
				else if( AlphaL >= 4 ) fout[i+3] = fout[i+3]|(fout[i+3] >> 4);
			}
		}
		else if( image.bpp == 2 )
		{
			fout[i+1] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
			if( AlphaL >= 7 ) fout[i+1] = fout[i+1] ? 0xFF : 0x00;
			else if( AlphaL >= 4 ) fout[i+1] = fout[i+1]|(fout[i+3] >> 4);
		}
	}

	Image_AddRGBAToPack( target, level, size, fout );
	return true;
}

bool Image_DecompressRGBA( uint target, int level, int intformat, uint width, uint height, uint imageSize, const void* data )
{
	byte	*fin, *fout;
	int	i, size; 
	color16	*col;

	if( !data ) return false;
	fin = (byte *)data;

	size = width * height * image.curdepth * 4;
	image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, size );
	fout = image.tempbuffer;

	switch( PFDesc[intformat].format )
	{
	case PF_RGB_16:
		for( i = 0, col = (color16 *)fin; i < width * height; i++, col += sizeof( color16 ))
		{
			fout[(i<<2)+0] = col->r;
			fout[(i<<2)+1] = col->g;
			fout[(i<<2)+2] = col->b;
			fout[(i<<2)+3] = 255;
		}
		break;	
	case PF_RGB_24:
		for (i = 0; i < width * height; i++ )
		{
			fout[(i<<2)+0] = fin[i+0];
			fout[(i<<2)+1] = fin[i+1];
			fout[(i<<2)+2] = fin[i+2];
			fout[(i<<2)+3] = 255;
		}
		break;
	case PF_BGR_24:
		for (i = 0; i < width * height; i++ )
		{
			fout[(i<<2)+0] = fin[i+2];
			fout[(i<<2)+1] = fin[i+1];
			fout[(i<<2)+2] = fin[i+0];
			fout[(i<<2)+3] = 255;
		}
		break;
	case PF_RGBA_32:
		Mem_Copy( fout, fin, size );
		break;
	case PF_BGRA_32:
	case PF_ABGR_64:
		for( i = 0; i < width * height; i++ )
		{
			fout[i*4+0] = fin[i*4+2];
			fout[i*4+1] = fin[i*4+1];
			fout[i*4+2] = fin[i*4+0];
			fout[i*4+3] = fin[i*4+3];
		}
		break;
	default: return false;
	}

	Image_AddRGBAToPack( target, level, size, fout );
	return true;
}

void Image_DecompressDDS( const byte *buffer, uint target )
{
	int i, size = 0;
	int w = image.curwidth;
	int h = image.curheight;
	int d = image.curdepth;

	// filter by cubemap side
	if( image.filter != CB_HINT_NO && image.filter != target ) return;

	switch( image.type )
	{
	case PF_RGB_24:
	case PF_RGBA_32: 
	case PF_BGRA_32:
	case PF_ABGR_64: image.decompress = Image_DecompressRGBA; break;
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
	case PF_ARGB_32: image.decompress = Image_DecompressARGB; break;
	case PF_DXT1:
	case PF_DXT2:
	case PF_DXT3:
	case PF_DXT4:
	case PF_DXT5:
	case PF_RXGB: image.decompress = Image_DecompressDXT; break;
	case PF_ATI1N:
	case PF_ATI2N: image.decompress = Image_DecompressATI; break;
	case PF_R_16F:
	case PF_R_32F:
	case PF_GR_32F:
	case PF_GR_64F:
	case PF_ABGR_64F:
	case PF_ABGR_128F: image.decompress = Image_DecompressFloat; break;
	case PF_UNKNOWN: break;
	}

	for( i = 0; i < image.cur_mips; i++, buffer += size )
	{
		Image_SetPixelFormat( w, h, d );
		size = image.SizeOfFile;

		if(!image.decompress( target, i, image.type, w, h, size, buffer ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}
}

bool Image_ForceDecompress( void )
{
	int	w, h;

	// GL_ARB_texture_compression missing, force to decompress anyway
	if(!( image.cmd_flags & IL_DDS_HARDWARE )) return true;

	// non power of two images needs resample
	// but resample code not supported compressed images
	w = image.width, h = image.height;
	Image_RoundDimensions( &w, &h );
	if( w != image.width || h != image.height )
		return true;

	// extract cubemap side from complex image
	if( image.filter != CB_HINT_NO )
		return true;

	// FIXME: load it properly with gl loader
	switch( image.type )
	{
	case PF_RXGB: return true;	// g-cont. test it with GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ?
	case PF_ATI1N: return true;	// hey, how called your OpenGL extension, ATI ?
	case PF_ATI2N: return true;
	}

	return false;
}

/*
=============
Image_LoadDDS
=============
*/
bool Image_LoadDDS( const char *name, const byte *buffer, size_t filesize )
{
	dds_t		header;
	byte		*fin;
	uint		i;

	if( filesize < sizeof( dds_t ))
	{
		MsgDev( D_ERROR, "Image_LoadDDS: file (%s) have invalid size\n", name );
		return false;
	}

	Mem_Copy( &header, buffer, sizeof( dds_t ));
	SwapBlock((int *)&header, sizeof( dds_t ));

	if( header.dwIdent != DDSHEADER ) return false; // it's not a dds file, just skip it
	if( header.dwSize != sizeof(dds_t) - sizeof( uint )) // size of the structure (minus MagicNum)
	{
		MsgDev( D_ERROR, "LoadDDS: (%s) have corrupted header\n", name );
		return false;
	}
	if( header.dsPixelFormat.dwSize != sizeof(dds_pixf_t)) // size of the structure
	{
		MsgDev( D_ERROR, "LoadDDS: (%s) have corrupt pixelformat header\n", name );
		return false;
	}

	image.width = header.dwWidth;
	image.height = header.dwHeight;
	image.bits_count = header.dsPixelFormat.dwRGBBitCount;
	if( header.dwFlags & DDS_DEPTH) image.num_layers = header.dwDepth;
	if(!Image_ValidSize( name )) return false;

	Image_DXTGetPixelFormat( &header ); // and image type too :)
	Image_DXTAdjustVolume( &header );

	if( image.type == PF_UNKNOWN ) 
	{
		MsgDev( D_WARN, "LoadDDS: (%s) have unsupported compression type\n", name );
		return false;
	}

	image.size = Image_DXTCalcSize( name, &header, filesize - 128 ); 
	if( image.size == 0 ) return false; // just in case
	fin = (byte *)(buffer + sizeof( dds_t ));

	if( Image_ForceDecompress())
	{
		int	offset, numsides = 1;
		uint	target = 1;
		byte	*buf = fin;

		// if hardware loader is absent or image not power of two
		// or user want load current side from cubemap we run software decompressing
		if( image.flags & IMAGE_CUBEMAP ) numsides = 6;
		Image_SetPixelFormat( image.width, image.height, image.num_layers ); // setup
		image.size = image.ptr = 0;
		if( image.cmd_flags & IL_IGNORE_MIPS )
			image.cur_mips = 1;
		else image.cur_mips = image.num_mips;
		image.num_mips = 1;

		for( i = 0, offset = 0; i < numsides; i++, buf += offset )
		{
			Image_SetPixelFormat( image.curwidth, image.curheight, image.curdepth );
			offset = image.SizeOfFile; // move pointer

			Image_DecompressDDS( buf, target + i );
		}
		// now we can change type to RGBA
		if( image.filter != CB_HINT_NO ) image.flags &= ~IMAGE_CUBEMAP; // side extracted
		image.type = PF_RGBA_32;
	}
	else
	{
		// dds files will be uncompressed on a render. requires minimal of info for set this
		image.rgba = Mem_Alloc( Sys.imagepool, image.size ); 
		Mem_Copy( image.rgba, fin, image.size );
	}
	return true;
}

bool Image_SaveDDS( const char *name, rgbdata_t *pix )
{
	vfile_t	*file;	// virtual file

	if(FS_FileExists( name ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	file = VFS_Open( NULL, "w" );
	if( !file ) return false;
	if(!Image_DXTWriteHeader( file, pix ))
	{
		MsgDev( D_ERROR, "Image_SaveDDS: unsupported format %s\n", PFDesc[pix->type].name );
		VFS_Close( file );
		return false;
	}
	if(!Image_DXTWriteImage( file, pix ))
	{
		MsgDev( D_ERROR, "Image_SaveDDS: can't create dds file\n" );
		VFS_Close( file );
		return false;
	}
	
	FS_WriteFile( name, VFS_GetBuffer( file ), VFS_Tell( file ));
	VFS_Close( file );	// release virtual file

	return true;
}