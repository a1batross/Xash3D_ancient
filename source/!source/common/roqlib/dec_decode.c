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

// RoQ Decoder

// Based on RoQ format specifications published by Dr. Tim Ferguson
#include <stdlib.h>
#include <string.h>

#include "roqlib_shared.h"


#define int long	// Force 32-bit

// RoQ decoder internals
typedef unsigned char block8[8*8*3];
typedef unsigned char block4[4*4*3];
typedef unsigned char block2[2*2*3];
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned short ushort;

// Audio chunks can contain up to 1 second of stereo audio
#define AUDIO_SAMPLE_MAX 22050

typedef struct
{
	block8 cb4large[256];

	block4 cb4[256];
	block2 cb2[256];

	ulong maxVideo;

	const void *reader;

	uchar *videoMemory;		// The only actual allocated buffer

	uchar *videoChunk;
	uchar *videoBuffers[2];
	int currentBuffer;
	int frameNum;

	uchar audioChunk[AUDIO_SAMPLE_MAX*2];
	short audioSamples[AUDIO_SAMPLE_MAX*2];
} ROQ_INTERNAL_STRUCT;

#define ROQ_LIB_BUILD
#include "roqd.h"

roq_exports_t ex;
roq_imports_t host;

// MAXIMUM CHUNK SIZE DETERMINATION
// Worst-case = Quad (Quad Quad Quad Quad)
// 10 bits from markers
// 128 bits from data
// (Width x Height) x 138 = Max bits
// Add 10 bytes to pad
// While this is NOT within the normal RoQ specifications, it is
// consistant in output from the Id/Trilobyte encoder and SwitchBlade
ulong MaxVideoChunkSize(ulong width, ulong height)
{
	return (((width * height) * 138) / 8) + 10;
}


// Localized marker data
short marker;
ulong size;
short argument;

ulong width;
ulong height;

ulong rowScanWidth;

block2 *cb2;
block4 *cb4;
block8 *cb4large;


int ProcessInfoTag(roq_decompressor_t *dc)
{
	uchar data[8];

	if(size != 8)
	{
		printf("Corrupt info block\n");
		return ROQ_MARKER_EOF;	// Corrupt info block
	}

	if(dc->flags & ROQ_VIDEO_INITIALIZED)
	{
		printf("Video already ready\n");
		return ROQ_MARKER_EOF;	// Video already initialized
	}

	if(!host.ReadData(dc->local.reader, data, 8))
	{
		printf("No tag\n");
		return ROQ_MARKER_EOF;
	}

	// Parse data
	width = data[0] | (data[1] << 8);
	height = data[2] | (data[3] << 8);

	if(!width || !height)
	{
		printf("Bad dimensions\n");
		return ROQ_MARKER_EOF;
	}

	if((width & 15) || (height & 15))
	{
		printf("Bad dimensions 2\n");
		return ROQ_MARKER_EOF;	// Bad dimensions
	}

	// Initialize video.  Use one big pile of memory for simplicity's sake
	dc->local.maxVideo = MaxVideoChunkSize(width, height);
	dc->local.videoMemory = malloc(dc->local.maxVideo + (width*height*3*2));
	if(!dc->local.videoMemory)
	{
		printf("Not enough vmemory\n");
		return ROQ_MARKER_EOF;	// Not enough memory
	}

	// Localize memory pools
	dc->local.videoBuffers[0] = dc->local.videoMemory;
	dc->local.videoBuffers[1] = dc->local.videoMemory + (width*height*3);
	dc->local.videoChunk = dc->local.videoMemory + (width*height*3*2);

	dc->width = (ushort)width;
	dc->height = (ushort)height;

	// Set as initialized
	dc->flags |= ROQ_VIDEO_INITIALIZED;

	return ROQ_MARKER_INFO;
}

int ProcessMonoAudio(roq_decompressor_t *dc)
{
	short lastValue;
	short newValue;
	uchar *input;
	short *output;

	uchar v;
	short base;
	int sign;

	if(size > AUDIO_SAMPLE_MAX)
	{
		printf("Audio overflow\n");
		return ROQ_MARKER_EOF;	// Audio overflow
	}

	// Read data
	if(!host.ReadData(dc->local.reader, dc->local.audioChunk, size))
	{
		printf("Read failed\n");
		return ROQ_MARKER_EOF;
	}

	// Decode samples
	input = dc->local.audioChunk;
	dc->availableAudioSamples = size;

	output = dc->audioSamples;

	lastValue = argument;
	while(size)
	{
		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign)
			newValue = lastValue - (base*base);
		else
			newValue = lastValue + (base*base);

		*output++ = lastValue = newValue;
		size--;
	}

	return ROQ_MARKER_AUDIO_MONO;
}

int ProcessStereoAudio(roq_decompressor_t *dc)
{
	short lastValueL;
	short lastValueR;
	short newValue;
	uchar *input;
	short *output;

	uchar v;
	short base;
	int sign;

	if(size & 1)
	{
		printf("Corrupt stereo audio\n");
		return ROQ_MARKER_EOF;	// Oops
	}

	size /= 2;

	if(size > AUDIO_SAMPLE_MAX)
	{
		printf("Audio overflow\n");
		return ROQ_MARKER_EOF;	// Audio overflow
	}

	// Read data
	if(!host.ReadData(dc->local.reader, dc->local.audioChunk, size*2))
	{
		printf("Not enough audio\n");
		return ROQ_MARKER_EOF;
	}

	// Decode samples
	input = dc->local.audioChunk;
	dc->availableAudioSamples = size;

	output = dc->audioSamples;

	lastValueL = argument & 0xFF00;
	lastValueR = (argument & 255) << 8;
	while(size)
	{
		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign)
			newValue = lastValueL - (base*base);
		else
			newValue = lastValueL + (base*base);

		*output++ = lastValueL = newValue;

		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign)
			newValue = lastValueR - (base*base);
		else
			newValue = lastValueR + (base*base);

		*output++ = lastValueR = newValue;

		size--;
	}

	return ROQ_MARKER_AUDIO_STEREO;
}

void YUVtoRGB(int y, int u, int v, uchar *r, uchar *g, uchar *b)
{
	int rc,gc,bc;

	u -= 128;
	v -= 128;

	rc = (y<<6) + (90*(int)v);
	gc = (y<<6) - (22*(int)u) - (46*(int)v);
	bc = (y<<6) + (113*(int)u);

	if(rc < 0) *r = 0; else if(rc > 16320) *r = 255; else *r = (uchar)(rc>>6);
	if(gc < 0) *g = 0; else if(gc > 16320) *g = 255; else *g = (uchar)(gc>>6);
	if(bc < 0) *b = 0; else if(bc > 16320) *b = 255; else *b = (uchar)(bc>>6);
}


int ProcessCodebooks(roq_decompressor_t *dc)
{
	ulong block;

	uchar block2data[1536];
	uchar block4data[1024];

	uchar *input;
	uchar *output;

	uchar y1,y2,y3,y4,u,v;

	// Parse out count
	ulong blockCount2;
	ulong blockCount4;

	cb2 = dc->local.cb2;
	cb4 = dc->local.cb4;
	cb4large = dc->local.cb4large;

	blockCount2 = (argument >> 8) & 255;
	blockCount4 = argument & 255;
	if(!blockCount2) blockCount2 = 256;
	if(!blockCount4 && blockCount2 * 6 < size) blockCount4 = 256;

	printf("Count2: %i  Count4: %i  Size: %i\n", blockCount2, blockCount4, size);

	// Read in the codebook
	if(size > blockCount2*6 + blockCount4*4)
	{
		printf("Bad codebook count");
		return ROQ_MARKER_EOF;	// Too small to be possible
	}
	if(!host.ReadData(dc->local.reader, block2data, blockCount2*6))
	{
		printf("Read failed on cb2");
		return ROQ_MARKER_EOF;
	}
	if(!host.ReadData(dc->local.reader, block4data, blockCount4*4))
	{
		printf("Read failed on cb4");
		return ROQ_MARKER_EOF;
	}

	// Decode the 2x2 blocks
	input = block2data;
	for(block=0;block<blockCount2;block++)
	{
		y1 = *input++;
		y2 = *input++;
		y3 = *input++;
		y4 = *input++;
		u = *input++;
		v = *input++;

		// Decode YUV values
		output = cb2[block];

		// 0  1  2    3  4  5
		// 6  7  8    9  10 11
		YUVtoRGB(y1, u, v, output, output+1, output+2);
		YUVtoRGB(y2, u, v, output+3, output+4, output+5);
		YUVtoRGB(y3, u, v, output+6, output+7, output+8);
		YUVtoRGB(y4, u, v, output+9, output+10, output+11);
	}

	// Decode the 4x4 blocks
	input = block4data;
	for(block=0;block<blockCount4;block++)
	{
		y1 = *input++;
		y2 = *input++;
		y3 = *input++;
		y4 = *input++;

		// Blit the cb2 blocks on to the cb4 blocks
		output = cb4[block];

		Blit(cb2[y1], output, 6, 6, 12, 2);
		Blit(cb2[y2], output+6, 6, 6, 12, 2);
		Blit(cb2[y3], output+(12*2), 6, 6, 12, 2);
		Blit(cb2[y4], output+(12*2)+6, 6, 6, 12, 2);

		// Create 8x8 blocks
		DoubleSize(output, cb4large[block], 4);
	}

	return ROQ_MARKER_CODEBOOK;
}


#define GETBYTE(x) if(!available) { return ROQ_MARKER_EOF; } x = *vidinput++; available--
#define GETWORD(x) if(available < 2) { return ROQ_MARKER_EOF; } x = (*vidinput++); x |= (*vidinput++) << 8; available -= 2
#define UNSPOOL(x) \
	if(parameterBits==0)\
	{\
		GETWORD(parameterWord);\
		parameterBits=16;\
	}\
	parameterBits -= 2;\
	x = (parameterWord >> parameterBits) & 3;

#define BLOCKMARK_SKIP   0
#define BLOCKMARK_MOTION 1
#define BLOCKMARK_VECTOR 2
#define BLOCKMARK_QUAD   3

// The soul of the machine: The RoQ video decompressor

uchar *frontBuffer;
uchar *backBuffer;

uchar *vidinput;
ulong available;

ushort parameterWord;
ushort parameterBits;

char dx,dy;

int ProcessQuad2(long x, long y)
{
	uchar blockid;
	GETBYTE(blockid);

	// Blit in a 2x2 block
	Blit(cb2[blockid], frontBuffer+(x*3)+(rowScanWidth*y), 2*3, 2*3, rowScanWidth, 2);
	return ROQ_MARKER_VIDEO;
}

int ProcessQuad4(int x, int y)
{
	int mx,my;
	int blockmark;
	int movement;
	uchar blockid;

	UNSPOOL(blockmark);

	switch(blockmark)
	{
	case BLOCKMARK_SKIP:
		//Blit(whiteOut4, frontBuffer+(x*3)+(y*rowScanWidth), 4*3, 4*3, rowScanWidth, 4);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_MOTION:
		GETBYTE(movement);

		// Decode offsets
		mx = x + 8 - (int)(movement >> 4) - (int)dx;
		my = y + 8 - (int)(movement & 15) - (int)dy;

		if(my < 0 || mx < 0 || (ulong)mx > width-4 || (ulong)my > height-4)
		{
			printf("Bad motion");
			return ROQ_MARKER_EOF;	// Out of bounds
		}

		// Blit the entire block
		Blit(backBuffer+(mx*3)+(my*rowScanWidth), frontBuffer+(x*3)+(y*rowScanWidth), 4*3, rowScanWidth, rowScanWidth, 4);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_VECTOR:
		GETBYTE(blockid);

		// Blit a 4x4 block
		Blit(cb4[blockid], frontBuffer+(x*3)+(y*rowScanWidth), 4*3, 4*3, rowScanWidth, 4);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_QUAD:
		// Process 4 subsection quads
		if(ProcessQuad2(x, y) == ROQ_MARKER_EOF)
		{
			printf("Quad2 failed");
			return ROQ_MARKER_EOF;
		}
		if(ProcessQuad2(x+2, y) == ROQ_MARKER_EOF)
		{
			printf("Quad2 failed 2");
			return ROQ_MARKER_EOF;
		}
		if(ProcessQuad2(x, y+2) == ROQ_MARKER_EOF)
		{
			printf("Quad2 failed 3");
			return ROQ_MARKER_EOF;
		}
		if(ProcessQuad2(x+2, y+2) == ROQ_MARKER_EOF)
		{
			printf("Quad2 failed 4");
			return ROQ_MARKER_EOF;
		}
		return ROQ_MARKER_VIDEO;
	};
	return ROQ_MARKER_EOF;	// Shut up compiler warning
}

int ProcessQuad8(long x, long y)
{
	int mx,my;
	int blockmark;
	uchar movement;
	uchar blockid;

	UNSPOOL(blockmark);

	switch(blockmark)
	{
	case BLOCKMARK_SKIP:
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_MOTION:
		GETBYTE(movement);

		// Decode offsets
		mx = x + 8 - ((movement >> 4) & 15) - (int)dx;
		my = y + 8 - (movement & 15) - (int)dy;

		if(mx < 0 || my < 0 || (ulong)mx > width-8 || (ulong)my > height-8)
		{
			printf("Quad8 out of bounds");
			return ROQ_MARKER_EOF;	// Out of bounds
		}

		// Blit the entire block
		Blit(backBuffer+(mx*3)+(my*rowScanWidth), frontBuffer+(x*3)+(y*rowScanWidth), 8*3, rowScanWidth, rowScanWidth, 8);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_VECTOR:
		GETBYTE(blockid);

		// Blit an oversized 4x4 block
		Blit(cb4large[blockid], frontBuffer+(x*3)+(y*rowScanWidth), 8*3, 8*3, rowScanWidth, 8);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_QUAD:
		// Process 4 subsection quads
		if(ProcessQuad4(x, y) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ProcessQuad4(x+4, y) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ProcessQuad4(x, y+4) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ProcessQuad4(x+4, y+4) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		return ROQ_MARKER_VIDEO;
	};
	return ROQ_MARKER_EOF;	// Shut up compiler warning
}

int ProcessQuad16(long x, long y)
{
	// Process 8x8 quads
	if(ProcessQuad8(x, y) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ProcessQuad8(x+8, y) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ProcessQuad8(x, y+8) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ProcessQuad8(x+8, y+8) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	return ROQ_MARKER_VIDEO;
}

int ProcessVideo(roq_decompressor_t *dc)
{
	long x,y;
	parameterBits = 0;

	dc->local.frameNum++;

	if(!dc->flags & ROQ_VIDEO_INITIALIZED)
		return ROQ_MARKER_EOF;	// Can't decompress without video initialized

	available = size;
	if(size > dc->local.maxVideo)
		return ROQ_MARKER_EOF;	// Too much video data
	if(!host.ReadData(dc->local.reader, dc->local.videoChunk, size))
		return ROQ_MARKER_EOF;	// Not enough real video data

	// Select buffers (and swap)
	frontBuffer = dc->local.videoBuffers[dc->local.currentBuffer];
	dc->local.currentBuffer = !dc->local.currentBuffer;
	backBuffer = dc->local.videoBuffers[dc->local.currentBuffer];


	// Blit the entire previous image to the backbuffer if this is the second frame
	// This is a stupid hack to keep some Id/Trilobyte encoded videos from screwing
	// up when they use SKIP markers in the second frame despite the fact that the
	// backbuffer should be empty
	if(dc->local.frameNum == 2)
		memcpy(frontBuffer, backBuffer, width*height*3);

	width = dc->width;
	height = dc->height;
	rowScanWidth = width * 3;

	cb2 = dc->local.cb2;
	cb4 = dc->local.cb4;
	cb4large = dc->local.cb4large;

	// Parse out average motion

	dx = ((argument >> 8) & 255);
	dy = (argument & 255);

	vidinput = dc->local.videoChunk;
	dc->rgb = frontBuffer;

	for(y=0;y<(long)height;y+=16)
		for(x=0;x<(long)width;x+=16)
		{
			if(ProcessQuad16(x,y) == ROQ_MARKER_EOF)
				return ROQ_MARKER_EOF;
		}

	return ROQ_MARKER_VIDEO;
}

int DecompressFrame(roq_decompressor_t *dc)
{
	uchar blockHeader[8];

	if(!host.ReadData(dc->local.reader, blockHeader, 8))
		return ROQ_MARKER_EOF;

	marker = (blockHeader[0] | (blockHeader[1] << 8));
	size = (blockHeader[2] | (blockHeader[3] << 8) | (blockHeader[4] << 16) | (blockHeader[5] << 24));
	argument = (blockHeader[6] | (blockHeader[7] << 8));

	switch(marker)
	{
	case ROQ_MARKER_INFO:
		return ProcessInfoTag(dc);
	case ROQ_MARKER_AUDIO_MONO:
		return ProcessMonoAudio(dc);
	case ROQ_MARKER_AUDIO_STEREO:
		return ProcessStereoAudio(dc);
	case ROQ_MARKER_CODEBOOK:
		return ProcessCodebooks(dc);
	case ROQ_MARKER_VIDEO:
		return ProcessVideo(dc);
	default:
		return ROQ_MARKER_EOF;	// Unknown
	}
}





// CreateDecompressor -- Creates an RoQ decompressor object
roq_decompressor_t *CreateDecompressor(const void *fileReader)
{
	uchar junk[8];
	roq_decompressor_t *dc;

	// Skip the first 8 bytes of the file
	if(!host.ReadData(fileReader, junk, 8))
		return NULL;

	dc = malloc(sizeof(roq_decompressor_t));
	if(!dc)
		return NULL;

	dc->flags = 0;
	dc->local.currentBuffer = 0;
	dc->local.reader = fileReader;
	dc->local.frameNum = 0;
	dc->audioSamples = dc->local.audioSamples;

	return dc;
}

void DestroyDecompressor(roq_decompressor_t *dec)
{
	if(dec->flags & ROQ_VIDEO_INITIALIZED)
		free(dec->local.videoMemory);
	free(dec);
}


// roqAPIExchange -- Exchanges API calls with an external host
ROQEXPORT roq_exports_t *RoQDec_APIExchange(roq_imports_t *i)
{
	host = *i;

	ex.CreateDecompressor = CreateDecompressor;
	ex.DestroyDecompressor = DestroyDecompressor;
	ex.DecompressFrame = DecompressFrame;

	return &ex;
}
